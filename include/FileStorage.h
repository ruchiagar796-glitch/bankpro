#pragma once
// FileStorage: reads and writes the plain-text data files.
// It knows nothing about Bank; it only converts a StoredState <-> files.
//
// File format: one record per line, fields separated by '|'.
// Inside a field:  '\' -> "\\",  '|' -> "\|",  newline -> "\n",  CR -> "\r".
// Lines starting with '#' are comments. Money is integer paise.
#include <ctime>
#include <string>
#include <vector>

#include "BankAccount.h"
#include "CustomerServiceQueue.h"
#include "Transaction.h"

struct AccountRecord {
    long long accountNumber = 0;
    std::string holderName;
    AccountType type = AccountType::Savings;
    money::Paise balance = 0;
    std::time_t createdAt = 0;
};

struct Counters {
    long long nextAccountNumber = 1001;
    long long nextTransactionId = 1;
    long long nextTokenNumber = 1;
};

struct StoredState {
    std::vector<AccountRecord> accounts;
    std::vector<Transaction> transactions;
    std::vector<long long> reversedIds;
    std::vector<QueueToken> queue;
    Counters counters;
};

class FileStorage {
public:
    explicit FileStorage(std::string directory);

    // Missing/empty files -> empty state. Malformed content -> BankError(DataCorrupt).
    // Never modifies any file.
    StoredState load() const;

    // Writes every file to "<name>.tmp", checks success, then renames over the
    // real file. Throws BankError(StorageFailure) on any problem.
    void save(const StoredState& state) const;

    const std::string& directory() const noexcept { return directory_; }

private:
    std::string directory_;
};
