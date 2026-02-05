#pragma once

#include "net/server.hpp"
#include "auth/authenticator.hpp"
#include "ssl_context.hpp"
#include "config.hpp"
#include "pop3_session.hpp"
#include <memory>
#include <filesystem>

namespace email::pop3 {

class POP3Server {
public:
    POP3Server(const POP3Config& config,
               std::shared_ptr<Authenticator> auth,
               const std::filesystem::path& maildir_root);

    ~POP3Server();

    void start();
    void stop();

    bool is_running() const;

    // Configure TLS
    bool configure_tls(const TLSConfig& tls_config);

private:
    POP3Config config_;
    std::shared_ptr<Authenticator> auth_;
    std::filesystem::path maildir_root_;

    std::unique_ptr<Server<POP3Session>> plain_server_;
    std::unique_ptr<Server<POP3Session>> tls_server_;

    SSLContext ssl_context_;
    bool tls_configured_ = false;
};

}  // namespace email::pop3
