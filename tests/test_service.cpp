#include "BankService.h"
#include "test_framework.h"

using nlohmann::json;

TEST(service_validates_request_bodies) {
    Bank bank;
    BankService svc(bank);
    auto create = svc.createAccount({{"holderName", "Tester"}, {"accountType", "Savings"}, {"initialDeposit", "1000"}});
    CHECK_EQ(create.status, 201);
    CHECK_EQ(create.body["data"]["accountNumber"].get<long long>(), 1001LL);

    CHECK_EQ(svc.deposit(json::object()).status, 400);                                           // missing fields
    CHECK_EQ(svc.deposit({{"accountNumber", 1001}}).status, 400);                                // missing amount
    CHECK_EQ(svc.deposit({{"accountNumber", 1001}, {"amount", "abc"}}).status, 400);
    CHECK_EQ(svc.deposit({{"accountNumber", 1001}, {"amount", "-5"}}).status, 400);
    CHECK_EQ(svc.deposit({{"accountNumber", 1001}, {"amount", 0}}).status, 400);
    CHECK_EQ(svc.deposit({{"accountNumber", 1001}, {"amount", 1e30}}).status, 400);             // overflow-sized number
    CHECK_EQ(svc.deposit({{"accountNumber", -1}, {"amount", 5}}).status, 400);
    CHECK_EQ(svc.deposit({{"accountNumber", "12abc"}, {"amount", 5}}).status, 400);
    CHECK_EQ(svc.deposit({{"accountNumber", 1.5}, {"amount", 5}}).status, 400);
    CHECK_EQ(svc.deposit(json::array()).status, 400);                                            // body not an object
    CHECK_EQ(svc.deposit({{"accountNumber", 4040}, {"amount", 5}}).status, 404);
    CHECK_EQ(svc.withdraw({{"accountNumber", 1001}, {"amount", "5000"}}).status, 422);          // insufficient
    CHECK_EQ(svc.createAccount({{"holderName", ""}, {"accountType", "Savings"}, {"initialDeposit", "1000"}}).status, 400);
    CHECK_EQ(svc.createAccount({{"holderName", "X"}, {"accountType", "Gold"}, {"initialDeposit", "1000"}}).status, 400);
    CHECK_EQ(bank.findAccount(1001)->balance(), rs(1000));  // nothing above changed the balance
}

TEST(service_success_flow_and_error_shape) {
    Bank bank;
    BankService svc(bank);
    svc.createAccount({{"holderName", "A"}, {"accountType", "savings"}, {"initialDeposit", 1000}});
    svc.createAccount({{"holderName", "B"}, {"accountType", "Current"}, {"initialDeposit", "5000.50"}});
    auto dep = svc.deposit({{"accountNumber", "1001"}, {"amount", "10.5"}});
    CHECK_EQ(dep.status, 201);
    CHECK_EQ(dep.body["data"]["account"]["balance"].get<std::string>(), std::string("1010.50"));
    auto tr = svc.transfer({{"fromAccount", 1002}, {"toAccount", 1001}, {"amount", "0.50"}});
    CHECK_EQ(tr.status, 201);
    auto err = svc.getAccount("9999");
    CHECK_EQ(err.status, 404);
    CHECK(err.body["success"] == false);
    CHECK_EQ(err.body["error"]["code"].get<std::string>(), std::string("NOT_FOUND"));
    CHECK_EQ(svc.getAccount("abc").status, 400);
    CHECK_EQ(svc.sortedAccounts("colour", "").status, 400);
    CHECK_EQ(svc.sortedAccounts("balance", "desc").body["data"][0]["accountNumber"].get<long long>(), 1002LL);
    CHECK_EQ(svc.listAccounts("1002").body["data"].size(), static_cast<std::size_t>(1));
    CHECK_EQ(svc.reverse(json::object()).status, 201);
    CHECK_EQ(svc.dashboard().body["data"]["totalAccounts"].get<int>(), 2);
}
