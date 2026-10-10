#include "FileStorage.h"

#include <charconv>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <utility>

namespace fs = std::filesystem;

namespace {

const char* kAccountsFile = "accounts.txt";
const char* kTransactionsFile = "transactions.txt";
const char* kMetadataFile = "metadata.txt";
const char* kReversedFile = "reversed_transactions.txt";
const char* kQueueFile = "customer_queue.txt";

[[noreturn]] void corrupt(const std::string& where, const std::string& what) {
    throw BankError(ErrorCode::DataCorrupt, where + ": " + what);
}

std::string escapeField(const std::string& s) {
    std::string r;
    for (char c : s) {
        if (c == '\\') r += "\\\\";
        else if (c == '|') r += "\\|";
        else if (c == '\n') r += "\\n";
        else if (c == '\r') r += "\\r";
        else r += c;
    }
    return r;
}

std::vector<std::string> splitLine(const std::string& line, const std::string& where) {
    std::vector<std::string> fields(1);
    for (std::size_t i = 0; i < line.size(); ++i) {
        const char c = line[i];
        if (c == '\\') {
            if (++i >= line.size()) corrupt(where, "dangling escape character");
            const char n = line[i];
            if (n == '\\' || n == '|') fields.back() += n;
            else if (n == 'n') fields.back() += '\n';
            else if (n == 'r') fields.back() += '\r';
            else corrupt(where, "unknown escape sequence");
        } else if (c == '|') {
            fields.emplace_back();
        } else {
            fields.back() += c;
        }
    }
    return fields;
}

long long toInt(const std::string& s, const std::string& where, const char* field) {
    long long v = 0;
    const char* b = s.data();
    const char* e = b + s.size();
    const auto r = std::from_chars(b, e, v);
    if (s.empty() || r.ec != std::errc() || r.ptr != e)
        corrupt(where, std::string("field '") + field + "' is not a valid integer");
    return v;
}

struct Line { std::size_t number; std::string text; };

std::vector<Line> readLines(const fs::path& path) {
    std::vector<Line> out;
    std::error_code ec;
    if (!fs::exists(path, ec)) return out;  // missing file = empty data
    std::ifstream in(path, std::ios::binary);
    if (!in) throw BankError(ErrorCode::StorageFailure, "Cannot open " + path.string() + " for reading");
    std::string text;
    std::size_t n = 0;
    while (std::getline(in, text)) {
        ++n;
        if (!text.empty() && text.back() == '\r') text.pop_back();
        if (text.empty() || text[0] == '#') continue;
        out.push_back({n, text});
    }
    if (in.bad()) throw BankError(ErrorCode::StorageFailure, "Error while reading " + path.string());
    return out;
}

std::string where(const char* file, const Line& l) {
    return std::string(file) + " line " + std::to_string(l.number);
}

void writeTemp(const fs::path& tmp, const std::string& content) {
    std::ofstream out(tmp, std::ios::binary | std::ios::trunc);
    if (!out) throw BankError(ErrorCode::StorageFailure, "Cannot open " + tmp.string() + " for writing");
    out.write(content.data(), static_cast<std::streamsize>(content.size()));
    out.flush();
    if (!out) throw BankError(ErrorCode::StorageFailure, "Failed to write " + tmp.string());
    out.close();
    if (out.fail()) throw BankError(ErrorCode::StorageFailure, "Failed to close " + tmp.string());
}

void replaceFile(const fs::path& tmp, const fs::path& target) {
    std::error_code ec;
    fs::rename(tmp, target, ec);
    if (!ec) return;
    fs::remove(target, ec);  // some platforms refuse to rename over an existing file
    ec.clear();
    fs::rename(tmp, target, ec);
    if (ec) throw BankError(ErrorCode::StorageFailure, "Cannot replace " + target.string() + ": " + ec.message());
}

}  // namespace

FileStorage::FileStorage(std::string directory) : directory_(std::move(directory)) {}

StoredState FileStorage::load() const {
    StoredState st;
    const fs::path dir(directory_);
    std::error_code ec;
    fs::create_directories(dir, ec);
    if (ec) throw BankError(ErrorCode::StorageFailure, "Cannot create data directory: " + ec.message());

    for (const Line& l : readLines(dir / kAccountsFile)) {
        const std::string w = where(kAccountsFile, l);
        const auto f = splitLine(l.text, w);
        if (f.size() != 5) corrupt(w, "expected 5 fields");
        AccountRecord r;
        r.accountNumber = toInt(f[0], w, "accountNumber");
        r.holderName = f[1];
        if (!parseAccountType(f[2], r.type)) corrupt(w, "unknown account type");
        r.balance = toInt(f[3], w, "balance");
        r.createdAt = static_cast<std::time_t>(toInt(f[4], w, "createdAt"));
        if (r.accountNumber <= 0) corrupt(w, "account number must be positive");
        if (r.balance < 0 || r.balance > money::kMaxBalancePaise) corrupt(w, "balance out of range");
        if (r.createdAt < 0) corrupt(w, "invalid creation time");
        st.accounts.push_back(std::move(r));
    }

    for (const Line& l : readLines(dir / kTransactionsFile)) {
        const std::string w = where(kTransactionsFile, l);
        const auto f = splitLine(l.text, w);
        if (f.size() != 8) corrupt(w, "expected 8 fields");
        TransactionType type;
        if (!parseTransactionType(f[2], type)) corrupt(w, "unknown transaction type");
        const long long id = toInt(f[0], w, "id");
        const long long acc = toInt(f[1], w, "accountNumber");
        const long long amount = toInt(f[3], w, "amount");
        const long long ts = toInt(f[4], w, "timestamp");
        const long long after = toInt(f[5], w, "balanceAfter");
        const long long related = toInt(f[6], w, "relatedId");
        if (id <= 0 || acc <= 0) corrupt(w, "id and account number must be positive");
        if (amount <= 0 || amount > money::kMaxTransactionPaise) corrupt(w, "amount out of range");
        if (after < 0 || after > money::kMaxBalancePaise) corrupt(w, "balanceAfter out of range");
        if (ts < 0 || related < 0) corrupt(w, "negative timestamp or related id");
        st.transactions.emplace_back(id, acc, type, amount, static_cast<std::time_t>(ts), after, f[7], related);
    }

    for (const Line& l : readLines(dir / kReversedFile)) {
        const std::string w = where(kReversedFile, l);
        const auto f = splitLine(l.text, w);
        if (f.size() != 1) corrupt(w, "expected 1 field");
        const long long id = toInt(f[0], w, "transactionId");
        if (id <= 0) corrupt(w, "transaction id must be positive");
        st.reversedIds.push_back(id);
    }

    for (const Line& l : readLines(dir / kQueueFile)) {
        const std::string w = where(kQueueFile, l);
        const auto f = splitLine(l.text, w);
        if (f.size() != 4) corrupt(w, "expected 4 fields");
        QueueToken t;
        t.number = toInt(f[0], w, "token");
        t.customerName = f[1];
        t.purpose = f[2];
        t.issuedAt = static_cast<std::time_t>(toInt(f[3], w, "issuedAt"));
        if (t.number <= 0 || t.issuedAt < 0 || t.customerName.empty()) corrupt(w, "invalid token record");
        st.queue.push_back(std::move(t));
    }

    for (const Line& l : readLines(dir / kMetadataFile)) {
        const std::string w = where(kMetadataFile, l);
        const auto eq = l.text.find('=');
        if (eq == std::string::npos) corrupt(w, "expected key=value");
        const std::string key = l.text.substr(0, eq);
        const long long v = toInt(l.text.substr(eq + 1), w, key.c_str());
        if (v <= 0) corrupt(w, "counter must be positive");
        if (key == "next_account_number") st.counters.nextAccountNumber = v;
        else if (key == "next_transaction_id") st.counters.nextTransactionId = v;
        else if (key == "next_token_number") st.counters.nextTokenNumber = v;
        // unknown keys are ignored so newer files do not break older builds
    }
    return st;
}

void FileStorage::save(const StoredState& st) const {
    const fs::path dir(directory_);
    std::error_code ec;
    fs::create_directories(dir, ec);
    if (ec) throw BankError(ErrorCode::StorageFailure, "Cannot create data directory: " + ec.message());

    std::ostringstream accounts, transactions, reversed, queue, meta;
    accounts << "# accountNumber|holderName|type|balancePaise|createdAt(epoch)\n";
    for (const auto& a : st.accounts)
        accounts << a.accountNumber << '|' << escapeField(a.holderName) << '|' << toString(a.type)
                 << '|' << a.balance << '|' << static_cast<long long>(a.createdAt) << '\n';

    transactions << "# id|accountNumber|type|amountPaise|timestamp(epoch)|balanceAfterPaise|relatedId|description\n";
    for (const auto& t : st.transactions)
        transactions << t.id() << '|' << t.accountNumber() << '|' << toString(t.type()) << '|'
                     << t.amount() << '|' << static_cast<long long>(t.timestamp()) << '|'
                     << t.balanceAfter() << '|' << t.relatedId() << '|' << escapeField(t.description()) << '\n';

    reversed << "# transactionId of every transaction that has already been reversed\n";
    for (long long id : st.reversedIds) reversed << id << '\n';

    queue << "# token|customerName|purpose|issuedAt(epoch)\n";
    for (const auto& q : st.queue)
        queue << q.number << '|' << escapeField(q.customerName) << '|' << escapeField(q.purpose) << '|'
              << static_cast<long long>(q.issuedAt) << '\n';

    meta << "# counters that keep IDs unique across restarts\n"
         << "next_account_number=" << st.counters.nextAccountNumber << '\n'
         << "next_transaction_id=" << st.counters.nextTransactionId << '\n'
         << "next_token_number=" << st.counters.nextTokenNumber << '\n';

    const std::vector<std::pair<const char*, std::string>> files = {
        {kTransactionsFile, transactions.str()}, {kReversedFile, reversed.str()},
        {kQueueFile, queue.str()},               {kAccountsFile, accounts.str()},
        {kMetadataFile, meta.str()}};

    // Phase 1: write every temp file. If anything fails, the real files are untouched.
    std::vector<fs::path> temps;
    try {
        for (const auto& f : files) {
            fs::path tmp = dir / (std::string(f.first) + ".tmp");
            temps.push_back(tmp);
            writeTemp(tmp, f.second);
        }
    } catch (...) {
        for (const auto& t : temps) { std::error_code e; fs::remove(t, e); }
        throw;
    }
    // Phase 2: replace the real files. (A crash exactly here can leave files from
    // two different saves - see README "Known limitations".)
    for (std::size_t i = 0; i < files.size(); ++i) replaceFile(temps[i], dir / files[i].first);
}
