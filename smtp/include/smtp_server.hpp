#pragma once

#include "net/server.hpp"
#include "auth/authenticator.hpp"
#include "ssl_context.hpp"
#include "config.hpp"
#include "smtp_session.hpp"
#include "smtp_relay.hpp"
#include <memory>
#include <filesystem>

namespace email::smtp {

class SMTPServer {
public:
    SMTPServer(const SMTPConfig& config,
               std::shared_ptr<Authenticator> auth,
               const std::filesystem::path& maildir_root);

    ~SMTPServer();

    void start();
    void stop();

    bool is_running() const;

    // Configure TLS
    bool configure_tls(const TLSConfig& tls_config);

private:
    SMTPConfig config_;
    std::shared_ptr<Authenticator> auth_;
    std::filesystem::path maildir_root_;

    std::shared_ptr<SMTPRelay> relay_;

    std::unique_ptr<Server<SMTPSession>> smtp_server_;      // Port 25
    std::unique_ptr<Server<SMTPSession>> submission_server_; // Port 587
    std::unique_ptr<Server<SMTPSession>> smtps_server_;      // Port 465

    std::unique_ptr<asio::io_context> relay_io_context_;

    SSLContext ssl_context_;
    bool tls_configured_ = false;
};

}  // namespace email::smtp
