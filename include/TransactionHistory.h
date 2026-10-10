#pragma once
// TransactionHistory: keeps every transaction in the order it happened.
#include <cstddef>
#include <vector>

#include "Transaction.h"

class TransactionHistory {
public:
    void add(const Transaction& tx) { records_.push_back(tx); }
    void clear() { records_.clear(); }
    std::size_t size() const noexcept { return records_.size(); }
    const std::vector<Transaction>& all() const noexcept { return records_; }

    const Transaction* findById(long long id) const;
    // Newest first. limit == 0 means "no limit".
    std::vector<Transaction> recent(std::size_t limit) const;
    std::vector<Transaction> forAccount(long long accountNumber, std::size_t limit = 0) const;

private:
    std::vector<Transaction> records_;
};
