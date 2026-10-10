#include "TransactionHistory.h"

const Transaction* TransactionHistory::findById(long long id) const {
    for (const Transaction& t : records_)
        if (t.id() == id) return &t;
    return nullptr;
}

std::vector<Transaction> TransactionHistory::recent(std::size_t limit) const {
    std::vector<Transaction> out;
    for (auto it = records_.rbegin(); it != records_.rend(); ++it) {
        if (limit != 0 && out.size() >= limit) break;
        out.push_back(*it);
    }
    return out;
}

std::vector<Transaction> TransactionHistory::forAccount(long long acc, std::size_t limit) const {
    std::vector<Transaction> out;
    for (auto it = records_.rbegin(); it != records_.rend(); ++it) {
        if (it->accountNumber() != acc) continue;
        if (limit != 0 && out.size() >= limit) break;
        out.push_back(*it);
    }
    return out;
}
