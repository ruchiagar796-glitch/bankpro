#pragma once
// ApiServer: maps HTTP routes to BankService calls and serves the frontend files.
// It contains NO banking logic. The HTTP library is hidden behind a forward
// declaration so only ApiServer.cpp has to compile the large httplib header.
#include <memory>
#include <string>

#include "BankService.h"

namespace httplib { class Server; }

class ApiServer {
public:
    ApiServer(BankService& service, std::string frontendDir);
    ~ApiServer();
    ApiServer(const ApiServer&) = delete;
    ApiServer& operator=(const ApiServer&) = delete;

    // Blocks until stop() is called. Returns false if the port could not be bound.
    bool listen(const std::string& host, int port);
    void stop();

    // Optional HTTP Basic login for the whole site (pages and API). /healthz stays open.
    void requireLogin(const std::string& user, const std::string& password);

private:
    void registerRoutes();
    BankService& service_;
    std::string frontendDir_;
    std::unique_ptr<httplib::Server> server_;
};
