#include "pop3_server.hpp"
#include "logger.hpp"

namespace email::pop3 {

POP3Server::POP3Server(const POP3Config& config,
                       std::shared_ptr<Authenticator> auth,
                       const std::filesystem::path& maildir_root)
    : config_(config)
    , auth_(std::move(auth))
    , maildir_root_(maildir_root) {
}

POP3Server::~POP3Server() {
    stop();
}

bool POP3Server::configure_tls(const TLSConfig& tls_config) {
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

void POP3Server::start() {
    // Create plain server
    plain_server_ = std::make_unique<Server<POP3Session>>(
        "POP3",
        config_.bind_address,
        config_.port,
        config_.thread_pool_size
    );

    // Set session factory
    plain_server_->set_session_factory(
        [this](asio::io_context& io_ctx, tcp::socket socket, ssl::context* /* ssl_ctx */) {
            auto session = std::make_shared<POP3Session>(
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

    LOG_INFO_FMT("Starting POP3 server on {}:{}", config_.bind_address, config_.port);
    plain_server_->start();

#ifdef ENABLE_TLS
    // Create TLS server if configured
    if (tls_configured_ && config_.tls_port > 0) {
        tls_server_ = std::make_unique<Server<POP3Session>>(
            "POP3S",
            config_.bind_address,
            config_.tls_port,
            ssl_context_.native(),
            config_.thread_pool_size
        );

        tls_server_->set_session_factory(
            [this](asio::io_context& io_ctx, tcp::socket socket, ssl::context* ssl_ctx) {
                if (ssl_ctx) {
                    return std::make_shared<POP3Session>(
                        io_ctx, std::move(socket), *ssl_ctx, auth_, maildir_root_, "mail.example.com"
                    );
                }
                return std::make_shared<POP3Session>(
                    io_ctx, std::move(socket), auth_, maildir_root_, "mail.example.com"
                );
            }
        );

        tls_server_->set_max_connections(config_.max_connections);
        tls_server_->set_connection_timeout(config_.connection_timeout);

        LOG_INFO_FMT("Starting POP3S server on {}:{}", config_.bind_address, config_.tls_port);
        tls_server_->start();
    }
#endif
}

void POP3Server::stop() {
    if (plain_server_) {
        plain_server_->stop();
        plain_server_.reset();
    }

    if (tls_server_) {
        tls_server_->stop();
        tls_server_.reset();
    }

    LOG_INFO("POP3 server stopped");
}

bool POP3Server::is_running() const {
    return (plain_server_ && plain_server_->is_running()) ||
           (tls_server_ && tls_server_->is_running());
}

}  // namespace email::pop3
