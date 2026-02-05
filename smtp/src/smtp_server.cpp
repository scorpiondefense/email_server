#include "smtp_server.hpp"
#include "logger.hpp"

namespace email::smtp {

SMTPServer::SMTPServer(const SMTPConfig& config,
                       std::shared_ptr<Authenticator> auth,
                       const std::filesystem::path& maildir_root)
    : config_(config)
    , auth_(std::move(auth))
    , maildir_root_(maildir_root) {

    relay_io_context_ = std::make_unique<asio::io_context>();
    relay_ = std::make_shared<SMTPRelay>(*relay_io_context_);
    relay_->set_hostname(config_.hostname);
}

SMTPServer::~SMTPServer() {
    stop();
}

bool SMTPServer::configure_tls(const TLSConfig& tls_config) {
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

void SMTPServer::start() {
    // Create SMTP server on port 25
    smtp_server_ = std::make_unique<Server<SMTPSession>>(
        "SMTP",
        config_.bind_address,
        config_.port,
        config_.thread_pool_size
    );

    smtp_server_->set_session_factory(
        [this](asio::io_context& io_ctx, tcp::socket socket, ssl::context* /* ssl_ctx */) {
            auto session = std::make_shared<SMTPSession>(
                io_ctx, std::move(socket), auth_, relay_, maildir_root_,
                config_.hostname, config_.local_domains
            );
            session->set_max_message_size(config_.max_message_size);
            session->set_max_recipients(config_.max_recipients);
            session->set_require_auth(config_.require_auth);
            session->set_allow_relay(config_.allow_relay);
#ifdef ENABLE_TLS
            if (tls_configured_) {
                session->set_ssl_context(&ssl_context_.native());
            }
#endif
            return session;
        }
    );

    smtp_server_->set_max_connections(config_.max_connections);
    smtp_server_->set_connection_timeout(config_.connection_timeout);

    LOG_INFO_FMT("Starting SMTP server on {}:{}", config_.bind_address, config_.port);
    smtp_server_->start();

    // Create submission server on port 587
    const uint16_t submission_port = 587;
    submission_server_ = std::make_unique<Server<SMTPSession>>(
        "SMTP-Submission",
        config_.bind_address,
        submission_port,
        config_.thread_pool_size
    );

    submission_server_->set_session_factory(
        [this](asio::io_context& io_ctx, tcp::socket socket, ssl::context* /* ssl_ctx */) {
            auto session = std::make_shared<SMTPSession>(
                io_ctx, std::move(socket), auth_, relay_, maildir_root_,
                config_.hostname, config_.local_domains
            );
            session->set_max_message_size(config_.max_message_size);
            session->set_max_recipients(config_.max_recipients);
            session->set_require_auth(true);  // Always require auth on submission
            session->set_allow_relay(true);   // Allow relay for authenticated users
#ifdef ENABLE_TLS
            if (tls_configured_) {
                session->set_ssl_context(&ssl_context_.native());
            }
#endif
            return session;
        }
    );

    submission_server_->set_max_connections(config_.max_connections);
    submission_server_->set_connection_timeout(config_.connection_timeout);

    LOG_INFO_FMT("Starting SMTP submission server on {}:{}", config_.bind_address, submission_port);
    submission_server_->start();

#ifdef ENABLE_TLS
    // Create SMTPS server on port 465 if TLS is configured
    if (tls_configured_ && config_.tls_port > 0) {
        smtps_server_ = std::make_unique<Server<SMTPSession>>(
            "SMTPS",
            config_.bind_address,
            config_.tls_port,
            ssl_context_.native(),
            config_.thread_pool_size
        );

        smtps_server_->set_session_factory(
            [this](asio::io_context& io_ctx, tcp::socket socket, ssl::context* ssl_ctx) {
                if (ssl_ctx) {
                    auto session = std::make_shared<SMTPSession>(
                        io_ctx, std::move(socket), *ssl_ctx, auth_, relay_, maildir_root_,
                        config_.hostname, config_.local_domains
                    );
                    session->set_max_message_size(config_.max_message_size);
                    session->set_max_recipients(config_.max_recipients);
                    session->set_require_auth(true);
                    session->set_allow_relay(true);
                    return session;
                }
                auto session = std::make_shared<SMTPSession>(
                    io_ctx, std::move(socket), auth_, relay_, maildir_root_,
                    config_.hostname, config_.local_domains
                );
                return session;
            }
        );

        smtps_server_->set_max_connections(config_.max_connections);
        smtps_server_->set_connection_timeout(config_.connection_timeout);

        LOG_INFO_FMT("Starting SMTPS server on {}:{}", config_.bind_address, config_.tls_port);
        smtps_server_->start();
    }
#endif
}

void SMTPServer::stop() {
    if (smtp_server_) {
        smtp_server_->stop();
        smtp_server_.reset();
    }

    if (submission_server_) {
        submission_server_->stop();
        submission_server_.reset();
    }

    if (smtps_server_) {
        smtps_server_->stop();
        smtps_server_.reset();
    }

    LOG_INFO("SMTP server stopped");
}

bool SMTPServer::is_running() const {
    return (smtp_server_ && smtp_server_->is_running()) ||
           (submission_server_ && submission_server_->is_running()) ||
           (smtps_server_ && smtps_server_->is_running());
}

}  // namespace email::smtp
