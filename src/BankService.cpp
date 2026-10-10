#include "BankService.h"

#include <cctype>
#include <exception>

using nlohmann::json;
using money::Paise;

namespace {

ServiceResult ok(const std::string& message, json data, int status = 200) {
    ServiceResult r;
    r.status = status;
    r.body = {{"success", true}, {"message", message}, {"data", std::move(data)}};
    return r;
}

[[noreturn]] void bad(const std::string& message) { throw BankError(ErrorCode::InvalidInput, message); }

long long parseAccountNumber(const std::string& text, const char* label = "Account number") {
    if (text.empty() || text.size() > 15) bad(std::string(label) + " must be a positive whole number");
    long long v = 0;
    for (char c : text) {
        if (!std::isdigit(static_cast<unsigned char>(c))) bad(std::string(label) + " must be a positive whole number");
        v = v * 10 + (c - '0');
    }
    if (v <= 0) bad(std::string(label) + " must be a positive whole number");
    return v;
}

void requireObject(const json& body) {
    if (!body.is_object()) bad("Request body must be a JSON object");
}

long long accountField(const json& body, const char* key) {
    requireObject(body);
    if (!body.contains(key) || body[key].is_null()) bad(std::string("Field '") + key + "' is required");
    const json& v = body[key];
    if (v.is_number_unsigned() || v.is_number_integer()) return parseAccountNumber(v.dump(), key);
    if (v.is_string()) return parseAccountNumber(v.get<std::string>(), key);
    bad(std::string("Field '") + key + "' must be a whole number");
}

std::string stringField(const json& body, const char* key, bool required) {
    requireObject(body);
    if (!body.contains(key) || body[key].is_null()) {
        if (required) bad(std::string("Field '") + key + "' is required");
        return "";
    }
    if (!body[key].is_string()) bad(std::string("Field '") + key + "' must be text");
    return body[key].get<std::string>();
}

Paise amountField(const json& body, const char* key) {
    requireObject(body);
    if (!body.contains(key) || body[key].is_null()) bad(std::string("Field '") + key + "' is required");
    const json& v = body[key];
    std::string text;
    if (v.is_string()) text = v.get<std::string>();
    else if (v.is_number()) text = v.dump();  // e.g. 100 or 100.5 ; huge values dump as 1e+30 and are rejected
    else bad(std::string("Field '") + key + "' must be a number");
    Paise out = 0;
    std::string err;
    if (!money::parseRupees(text, out, err)) bad(err);
    return out;
}

json accountJson(const BankAccount& a) {
    return {{"accountNumber", a.accountNumber()},
            {"holderName", a.holderName()},
            {"accountType", a.typeName()},
            {"balance", money::format(a.balance())},
            {"balancePaise", a.balance()},
            {"createdAt", Transaction(0, 0, TransactionType::Deposit, 1, a.createdAt(), 0, "").timestampString()}};
}

json txJson(const Transaction& t) {
    return {{"id", t.id()},
            {"accountNumber", t.accountNumber()},
            {"type", toString(t.type())},
            {"amount", money::format(t.amount())},
            {"amountPaise", t.amount()},
            {"balanceAfter", money::format(t.balanceAfter())},
            {"timestamp", t.timestampString()},
            {"description", t.description()},
            {"relatedId", t.relatedId()}};
}

json tokenJson(const QueueToken& q) {
    Transaction fmt(0, 0, TransactionType::Deposit, 1, q.issuedAt, 0, "");
    return {{"token", q.number}, {"customerName", q.customerName}, {"purpose", q.purpose},
            {"issuedAt", fmt.timestampString()}};
}

json accountList(const std::vector<const BankAccount*>& v) {
    json arr = json::array();
    for (const BankAccount* a : v) arr.push_back(accountJson(*a));
    return arr;
}

json txList(const std::vector<Transaction>& v) {
    json arr = json::array();
    for (const Transaction& t : v) arr.push_back(txJson(t));
    return arr;
}

}  // namespace

ServiceResult BankService::error(int status, const std::string& code, const std::string& message) {
    ServiceResult r;
    r.status = status;
    r.body = {{"success", false}, {"error", {{"code", code}, {"message", message}}}};
    return r;
}

template <typename F>
ServiceResult BankService::run(F&& action) {
    std::lock_guard<std::mutex> lock(mutex_);
    try {
        return action();
    } catch (const BankError& e) {
        return error(httpStatusFor(e.code()), errorCodeName(e.code()), e.what());
    } catch (const std::exception&) {
        return error(500, "INTERNAL_ERROR", "Unexpected server error");
    }
}

ServiceResult BankService::dashboard() {
    return run([&] {
        const Bank::Summary s = bank_.summary();
        return ok("Dashboard loaded",
                  {{"totalAccounts", s.accountCount},
                   {"totalBalance", money::format(s.totalBalance)},
                   {"totalBalancePaise", s.totalBalance},
                   {"totalTransactions", s.transactionCount},
                   {"customersWaiting", s.customersWaiting},
                   {"nextTokenNumber", bank_.nextTokenNumber()},
                   {"recentTransactions", txList(bank_.recentTransactions(5))},
                   {"notice", "Local educational simulation - not a real financial institution."}});
    });
}

ServiceResult BankService::accountTypes() {
    return run([&] {
        json arr = json::array();
        for (AccountType t : {AccountType::Savings, AccountType::Current}) {
            auto a = BankAccount::create(t, 1, "x", 0);
            arr.push_back({{"type", a->typeName()},
                           {"minimumOpeningDeposit", money::format(a->minimumOpeningDeposit())},
                           {"maxWithdrawalPerTransaction", money::format(a->maxWithdrawalPerTransaction())}});
        }
        return ok("Account types", arr);
    });
}

ServiceResult BankService::listAccounts(const std::string& search) {
    return run([&] {
        if (search.size() > 60) bad("Search text is too long");
        // A purely numeric search is treated as an account number, otherwise as a name fragment.
        bool numeric = !search.empty();
        for (char c : search) numeric = numeric && std::isdigit(static_cast<unsigned char>(c));
        if (numeric && search.size() <= 15) {
            const BankAccount* a = bank_.findAccount(parseAccountNumber(search));
            std::vector<const BankAccount*> v;
            if (a) v.push_back(a);
            return ok("Accounts loaded", accountList(v));
        }
        return ok("Accounts loaded", accountList(search.empty() ? bank_.allAccounts() : bank_.searchByName(search)));
    });
}

ServiceResult BankService::sortedAccounts(const std::string& by, const std::string& order) {
    return run([&] {
        SortKey key;
        if (by.empty() || by == "name") key = SortKey::Name;
        else if (by == "balance") key = SortKey::Balance;
        else bad("Sort field must be 'name' or 'balance'");
        if (!order.empty() && order != "asc" && order != "desc") bad("Sort order must be 'asc' or 'desc'");
        return ok("Accounts sorted with merge sort", accountList(bank_.sortedAccounts(key, order != "desc")));
    });
}

ServiceResult BankService::getAccount(const std::string& text) {
    return run([&] {
        const long long n = parseAccountNumber(text);
        const BankAccount* a = bank_.findAccount(n);
        if (!a) throw BankError(ErrorCode::NotFound, "Account " + std::to_string(n) + " not found");
        return ok("Account loaded", accountJson(*a));
    });
}

ServiceResult BankService::createAccount(const json& body) {
    return run([&] {
        const std::string name = stringField(body, "holderName", true);
        const std::string typeText = stringField(body, "accountType", true);
        AccountType type;
        if (!parseAccountType(typeText, type)) bad("Account type must be 'Savings' or 'Current'");
        const Paise initial = amountField(body, "initialDeposit");
        const BankAccount& a = bank_.createAccount(name, type, initial);
        return ok("Account " + std::to_string(a.accountNumber()) + " created", accountJson(a), 201);
    });
}

ServiceResult BankService::closeAccount(const std::string& text) {
    return run([&] {
        const long long n = parseAccountNumber(text);
        bank_.closeAccount(n);
        return ok("Account " + std::to_string(n) + " closed", {{"accountNumber", n}});
    });
}

ServiceResult BankService::deposit(const json& body) {
    return run([&] {
        const long long n = accountField(body, "accountNumber");
        const Paise amt = amountField(body, "amount");
        const Transaction t = bank_.deposit(n, amt);
        return ok("Deposited Rs " + money::format(amt) + " into account " + std::to_string(n),
                  {{"transaction", txJson(t)}, {"account", accountJson(*bank_.findAccount(n))}}, 201);
    });
}

ServiceResult BankService::withdraw(const json& body) {
    return run([&] {
        const long long n = accountField(body, "accountNumber");
        const Paise amt = amountField(body, "amount");
        const Transaction t = bank_.withdraw(n, amt);
        return ok("Withdrew Rs " + money::format(amt) + " from account " + std::to_string(n),
                  {{"transaction", txJson(t)}, {"account", accountJson(*bank_.findAccount(n))}}, 201);
    });
}

ServiceResult BankService::transfer(const json& body) {
    return run([&] {
        const long long from = accountField(body, "fromAccount");
        const long long to = accountField(body, "toAccount");
        const Paise amt = amountField(body, "amount");
        const auto legs = bank_.transfer(from, to, amt);
        return ok("Transferred Rs " + money::format(amt) + " from " + std::to_string(from) + " to " + std::to_string(to),
                  {{"debit", txJson(legs.first)}, {"credit", txJson(legs.second)},
                   {"fromAccount", accountJson(*bank_.findAccount(from))},
                   {"toAccount", accountJson(*bank_.findAccount(to))}}, 201);
    });
}

ServiceResult BankService::recentTransactions(const std::string& limitText) {
    return run([&] {
        std::size_t limit = 50;
        if (!limitText.empty()) {
            if (limitText.size() > 4) bad("limit must be a number between 1 and 1000");
            const long long v = parseAccountNumber(limitText, "limit");
            if (v > 1000) bad("limit must be a number between 1 and 1000");
            limit = static_cast<std::size_t>(v);
        }
        return ok("Transactions loaded", txList(bank_.recentTransactions(limit)));
    });
}

ServiceResult BankService::accountTransactions(const std::string& text) {
    return run([&] {
        const long long n = parseAccountNumber(text);
        if (!bank_.findAccount(n)) throw BankError(ErrorCode::NotFound, "Account " + std::to_string(n) + " not found");
        return ok("Transactions loaded", txList(bank_.transactionsFor(n)));
    });
}

ServiceResult BankService::nextReversible() {
    return run([&] {
        const auto t = bank_.peekReversible();
        return ok(t ? "Most recent reversible transaction" : "Nothing to reverse",
                  {{"transaction", t ? txJson(*t) : json(nullptr)}, {"stackSize", bank_.reversibleCount()}});
    });
}

ServiceResult BankService::reverse(const json& body) {
    return run([&] {
        long long id = 0;
        if (body.is_object() && body.contains("transactionId") && !body["transactionId"].is_null())
            id = accountField(body, "transactionId");
        else if (!body.is_object() && !body.is_null()) bad("Request body must be a JSON object");
        const Transaction t = bank_.reverseLastTransaction(id);
        return ok("Transaction #" + std::to_string(t.relatedId()) + " reversed",
                  {{"reversal", txJson(t)}, {"account", accountJson(*bank_.findAccount(t.accountNumber()))}}, 201);
    });
}

ServiceResult BankService::queue() {
    return run([&] {
        json arr = json::array();
        for (const QueueToken& q : bank_.queueSnapshot()) arr.push_back(tokenJson(q));
        return ok("Queue loaded", {{"waiting", arr}, {"count", arr.size()}, {"nextTokenNumber", bank_.nextTokenNumber()}});
    });
}

ServiceResult BankService::issueToken(const json& body) {
    return run([&] {
        const std::string name = stringField(body, "customerName", true);
        const std::string purpose = stringField(body, "purpose", false);
        const QueueToken t = bank_.issueToken(name, purpose);
        return ok("Token " + std::to_string(t.number) + " issued", tokenJson(t), 201);
    });
}

ServiceResult BankService::serveNext() {
    return run([&] {
        const QueueToken t = bank_.serveNextCustomer();
        return ok("Now serving token " + std::to_string(t.number), tokenJson(t));
    });
}
