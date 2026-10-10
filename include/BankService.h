#pragma once
// BankService: the layer between HTTP and Bank. It validates request data
// (JSON bodies, path/query text), calls Bank, and returns a ServiceResult
// {HTTP status, JSON body}. It has no dependency on the HTTP library.
// A mutex makes every call safe even if the HTTP server uses several threads.
#include <mutex>
#include <string>

#include <nlohmann/json.hpp>

#include "Bank.h"

struct ServiceResult {
    int status = 200;
    nlohmann::json body;
};

class BankService {
public:
    explicit BankService(Bank& bank) : bank_(bank) {}

    ServiceResult dashboard();
    ServiceResult accountTypes();
    ServiceResult listAccounts(const std::string& search);
    ServiceResult sortedAccounts(const std::string& by, const std::string& order);
    ServiceResult getAccount(const std::string& accountNumberText);
    ServiceResult createAccount(const nlohmann::json& body);
    ServiceResult closeAccount(const std::string& accountNumberText);

    ServiceResult deposit(const nlohmann::json& body);
    ServiceResult withdraw(const nlohmann::json& body);
    ServiceResult transfer(const nlohmann::json& body);
    ServiceResult recentTransactions(const std::string& limitText);
    ServiceResult accountTransactions(const std::string& accountNumberText);
    ServiceResult nextReversible();
    ServiceResult reverse(const nlohmann::json& body);

    ServiceResult queue();
    ServiceResult issueToken(const nlohmann::json& body);
    ServiceResult serveNext();

    // Helpers shared with the API layer.
    static ServiceResult error(int status, const std::string& code, const std::string& message);

private:
    template <typename F>
    ServiceResult run(F&& action);

    Bank& bank_;
    std::mutex mutex_;
};
