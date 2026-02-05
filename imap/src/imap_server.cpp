#include "imap_server.hpp"
#include "logger.hpp"

namespace email::imap {

IMAPServer::IMAPServer(const IMAPConfig& config,
                       std::shared_ptr<Authenticator> auth,
                       const std::filesystem::path& maildir_root)
    : config_(config)
    , auth_(std::move(auth))
    , maildir_root_(maildir_root) {
}

IMAPServer::~IMAPServer() {
    stop();
}

bool IMAPServer::configure_tls(const TLSConfig& tls_config) {
#ifdef ENABLE_TLS
    ssl_context_ = SSLContext::create_server_context(
        tls_config.certificate_file,
        tls_config.private_key_file,
        tls_config.ca_file,
        tls_config.ciphers
    );

    if (!ssl_context_.is_initialized()) {
        LOG_ERROR_FMT("Failed to configure TLS: {}", ssl_context_.last_error());
        return false;
    }

    tls_configured_ = true;
    return true;
#else
    (void)tls_config;
    LOG_WARNING("TLS support not compiled in");
    return false;
#endif
}

void IMAPServer::start() {
    // Create plain server
    plain_server_ = std::make_unique<Server<IMAPSession>>(
        "IMAP",
        config_.bind_address,
        config_.port,
        config_.thread_pool_size
    );

    // Set session factory
    plain_server_->set_session_factory(
        [this](asio::io_context& io_ctx, tcp::socket socket, ssl::context* /* ssl_ctx */) {
            auto session = std::make_shared<IMAPSession>(
                io_ctx, std::move(socket), auth_, maildir_root_, "mail.example.com"
            );
#ifdef ENABLE_TLS
            if (tls_configured_) {
                session->set_ssl_context(&ssl_context_.native());
            }
#endif
            return session;
        }
    );

    plain_server_->set_max_connections(config_.max_connections);
    plain_server_->set_connection_timeout(config_.connection_timeout);

    LOG_INFO_FMT("Starting IMAP server on {}:{}", config_.bind_address, config_.port);
    plain_server_->start();

#ifdef ENABLE_TLS
    // Create TLS server if configured
    if (tls_configured_ && config_.tls_port > 0) {
        tls_server_ = std::make_unique<Server<IMAPSession>>(
            "IMAPS",
            config_.bind_address,
            config_.tls_port,
            ssl_context_.native(),
            config_.thread_pool_size
        );

        tls_server_->set_session_factory(
            [this](asio::io_context& io_ctx, tcp::socket socket, ssl::context* ssl_ctx) {
                if (ssl_ctx) {
                    return std::make_shared<IMAPSession>(
                        io_ctx, std::move(socket), *ssl_ctx, auth_, maildir_root_, "mail.example.com"
                    );
                }
                return std::make_shared<IMAPSession>(
                    io_ctx, std::move(socket), auth_, maildir_root_, "mail.example.com"
                );
            }
        );

        tls_server_->set_max_connections(config_.max_connections);
        tls_server_->set_connection_timeout(config_.connection_timeout);

        LOG_INFO_FMT("Starting IMAPS server on {}:{}", config_.bind_address, config_.tls_port);
        tls_server_->start();
    }
#endif
}

void IMAPServer::stop() {
    if (plain_server_) {
        plain_server_->stop();
        plain_server_.reset();
    }

    if (tls_server_) {
        tls_server_->stop();
        tls_server_.reset();
    }

    LOG_INFO("IMAP server stopped");
}

bool IMAPServer::is_running() const {
    return (plain_server_ && plain_server_->is_running()) ||
           (tls_server_ && tls_server_->is_running());
}

}  // namespace email::imap
