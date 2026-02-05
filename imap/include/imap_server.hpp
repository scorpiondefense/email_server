#pragma once

#include "net/server.hpp"
#include "auth/authenticator.hpp"
#include "ssl_context.hpp"
#include "config.hpp"
#include "imap_session.hpp"
#include <memory>
#include <filesystem>

namespace email::imap {

class IMAPServer {
public:
    IMAPServer(const IMAPConfig& config,
               std::shared_ptr<Authenticator> auth,
               const std::filesystem::path& maildir_root);

    ~IMAPServer();

    void start();
    void stop();

    bool is_running() const;

    // Configure TLS
    bool configure_tls(const TLSConfig& tls_config);

private:
    IMAPConfig config_;
    std::shared_ptr<Authenticator> auth_;
    std::filesystem::path maildir_root_;

    std::unique_ptr<Server<IMAPSession>> plain_server_;
    std::unique_ptr<Server<IMAPSession>> tls_server_;

    SSLContext ssl_context_;
    bool tls_configured_ = false;
};

}  // namespace email::imap
