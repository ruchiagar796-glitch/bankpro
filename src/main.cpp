// Entry point: parses options, loads the bank from the data folder, starts the HTTP server.
// Usage: bank_server [--port 8080] [--data data] [--frontend frontend]
// Environment: PORT, BMS_HOST (default 127.0.0.1), BMS_DATA_DIR, BMS_USER, BMS_PASSWORD
#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <string>

#include "ApiServer.h"
#include "Bank.h"
#include "BankService.h"

int main(int argc, char** argv) {
    // Defaults can be overridden by environment variables (used by cloud hosts) and then by options.
    auto env = [](const char* name, const std::string& fallback) {
        const char* v = std::getenv(name);
        return (v && *v) ? std::string(v) : fallback;
    };
    std::string dataDir = env("BMS_DATA_DIR", "data");
    std::string frontendDir = "frontend";
    std::string host = env("BMS_HOST", "127.0.0.1");   // cloud: 0.0.0.0
    const std::string loginUser = env("BMS_USER", "admin");
    const std::string loginPassword = env("BMS_PASSWORD", "");
    int port = 8080;
    try { port = std::stoi(env("PORT", "8080")); } catch (...) { port = 0; }
    if (port < 1 || port > 65535) { std::cerr << "Invalid PORT value\n"; return 2; }

    for (int i = 1; i < argc; ++i) {
        const std::string a = argv[i];
        auto value = [&](std::string& target) {
            if (i + 1 >= argc) { std::cerr << "Missing value for " << a << "\n"; std::exit(2); }
            target = argv[++i];
        };
        if (a == "--port") {
            std::string p;
            value(p);
            try { port = std::stoi(p); } catch (...) { port = 0; }
            if (port < 1 || port > 65535) { std::cerr << "Invalid port\n"; return 2; }
        } else if (a == "--data") value(dataDir);
        else if (a == "--frontend") value(frontendDir);
        else { std::cerr << "Unknown option " << a << "\nUsage: bank_server [--port N] [--data DIR] [--frontend DIR]\n"; return 2; }
    }
    // Safety rule: never listen on a public address without a password.
    const bool loopback = (host == "127.0.0.1" || host == "localhost" || host == "::1");
    if (!loopback && loginPassword.empty()) {
        std::cerr << "Refusing to listen on " << host << " without a password.\n"
                  << "Set the BMS_PASSWORD environment variable (and optionally BMS_USER).\n";
        return 2;
    }
    // Convenience: allow running from inside the build folder.
    namespace fs = std::filesystem;
    if (!fs::exists(frontendDir) && fs::exists("../frontend")) frontendDir = "../frontend";

    try {
        Bank bank(dataDir);
        BankService service(bank);
        ApiServer server(service, frontendDir);
        if (!loginPassword.empty()) server.requireLogin(loginUser, loginPassword);

        std::cout << "Bank Management System (educational simulation)\n"
                  << "  data folder    : " << fs::absolute(dataDir).string() << "\n"
                  << "  frontend folder: " << fs::absolute(frontendDir).string() << "\n"
                  << "  listening on   : " << host << ":" << port << (loginPassword.empty() ? "  (no login)" : "  (login required)") << "\n"
                  << "  open in browser: http://localhost:" << port << "\n"
                  << "Press Ctrl+C to stop.\n";
        if (!server.listen(host, port)) {
            std::cerr << "Could not listen on port " << port << " (is it already in use? try --port 8081)\n";
            return 1;
        }
    } catch (const BankError& e) {
        std::cerr << "Startup failed: " << e.what() << "\n"
                  << "No data was modified. Fix or restore the data files and try again.\n";
        return 1;
    } catch (const std::exception& e) {
        std::cerr << "Unexpected error: " << e.what() << "\n";
        return 1;
    }
    return 0;
}
