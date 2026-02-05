#include "imap_server.hpp"
#include "config.hpp"
#include "logger.hpp"
#include "auth/authenticator.hpp"
#include <iostream>
#include <csignal>
#include <atomic>

namespace {
    std::atomic<bool> g_running{true};
    email::imap::IMAPServer* g_server = nullptr;
}

void signal_handler(int signal) {
    std::cout << "\nReceived signal " << signal << ", shutting down...\n";
    g_running = false;
    if (g_server) {
        g_server->stop();
    }
}

void print_usage(const char* program) {
    std::cout << "Usage: " << program << " [options]\n"
              << "Options:\n"
              << "  -c, --config <file>    Configuration file path\n"
              << "  -h, --help             Show this help message\n"
              << "  -v, --version          Show version information\n";
}

int main(int argc, char* argv[]) {
    std::string config_file = "/etc/email_server/server.conf";

    // Parse command line arguments
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "-h" || arg == "--help") {
            print_usage(argv[0]);
            return 0;
        } else if (arg == "-v" || arg == "--version") {
            std::cout << "IMAP Server v1.0.0\n";
            return 0;
        } else if ((arg == "-c" || arg == "--config") && i + 1 < argc) {
            config_file = argv[++i];
        }
    }

    // Initialize logger
    email::Logger::instance().init(
        email::LogLevel::Info,
        true,
        "/var/log/email_server/imap.log"
    );

    LOG_INFO("IMAP Server starting...");

    // Load configuration
    auto& config = email::Config::instance();
    if (!config.load(config_file)) {
        LOG_WARNING_FMT("Could not load config file: {}, using defaults", config_file);
    }

    // Initialize authenticator
    auto auth = std::make_shared<email::Authenticator>(config.database().path);
    if (!auth->initialize()) {
        LOG_FATAL("Failed to initialize authenticator");
        return 1;
    }

    // Create server
    email::imap::IMAPServer server(config.imap(), auth, config.storage().maildir_root);
    g_server = &server;

    // Configure TLS if available
    if (!config.tls().certificate_file.empty() && !config.tls().private_key_file.empty()) {
        if (!server.configure_tls(config.tls())) {
            LOG_WARNING("TLS configuration failed, continuing without TLS");
        }
    }

    // Set up signal handlers
    std::signal(SIGINT, signal_handler);
    std::signal(SIGTERM, signal_handler);

    // Start server
    try {
        server.start();
        LOG_INFO("IMAP server started successfully");

        // Wait for shutdown signal
        while (g_running && server.is_running()) {
            std::this_thread::sleep_for(std::chrono::seconds(1));
        }
    } catch (const std::exception& e) {
        LOG_FATAL_FMT("Server error: {}", e.what());
        return 1;
    }

    LOG_INFO("IMAP server shutdown complete");
    return 0;
}
