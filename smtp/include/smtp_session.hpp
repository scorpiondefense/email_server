#pragma once

#include "net/session.hpp"
#include "auth/authenticator.hpp"
#include "storage/maildir.hpp"
#include "smtp_commands.hpp"
#include "smtp_relay.hpp"
#include <memory>
#include <vector>
#include <set>

namespace email::smtp {

enum class SessionState {
    CONNECTED,      // Initial state
    GREETED,        // After HELO/EHLO
    MAIL,           // After MAIL FROM
    RCPT,           // After at least one RCPT TO
    DATA,           // During DATA reception
    QUIT            // After QUIT
};

enum class AuthState {
    NONE,
    PLAIN_WAITING,
    LOGIN_WAITING_USERNAME,
    LOGIN_WAITING_PASSWORD,
    AUTHENTICATED
};

struct Envelope {
    std::string mail_from;
    std::vector<std::string> rcpt_to;
    std::string data;

    void clear() {
        mail_from.clear();
        rcpt_to.clear();
        data.clear();
    }
};

class SMTPSession : public Session {
public:
    SMTPSession(asio::io_context& io_context, tcp::socket socket,
                std::shared_ptr<Authenticator> auth,
                std::shared_ptr<SMTPRelay> relay,
                const std::filesystem::path& maildir_root,
                const std::string& hostname,
                const std::vector<std::string>& local_domains);

#ifdef ENABLE_TLS
    SMTPSession(asio::io_context& io_context, tcp::socket socket,
                ssl::context& ssl_ctx,
                std::shared_ptr<Authenticator> auth,
                std::shared_ptr<SMTPRelay> relay,
                const std::filesystem::path& maildir_root,
                const std::string& hostname,
                const std::vector<std::string>& local_domains);
#endif

    ~SMTPSession() override = default;

    // State
    SessionState state() const { return state_; }
    void set_state(SessionState state) { state_ = state; }

    AuthState auth_state() const { return auth_state_; }
    void set_auth_state(AuthState state) { auth_state_ = state; }

    // Envelope access
    Envelope& envelope() { return envelope_; }
    const Envelope& envelope() const { return envelope_; }

    // Client identification
    const std::string& client_hostname() const { return client_hostname_; }
    void set_client_hostname(const std::string& hostname) { client_hostname_ = hostname; }

    // Authentication
    Authenticator& authenticator() { return *auth_; }
    bool is_authenticated() const override { return Session::is_authenticated(); }
    bool require_auth() const { return require_auth_; }
    void set_require_auth(bool require) { require_auth_ = require; }

    // Domain checking
    bool is_local_domain(const std::string& domain) const;
    const std::vector<std::string>& local_domains() const { return local_domains_; }

    // Relay
    SMTPRelay& relay() { return *relay_; }

    // Message delivery
    bool deliver_message();

    // Configuration
    const std::string& hostname() const { return hostname_; }
    size_t max_message_size() const { return max_message_size_; }
    void set_max_message_size(size_t size) { max_message_size_ = size; }
    size_t max_recipients() const { return max_recipients_; }
    void set_max_recipients(size_t max) { max_recipients_ = max; }
    bool allow_relay() const { return allow_relay_; }
    void set_allow_relay(bool allow) { allow_relay_ = allow; }

    // STARTTLS
    bool starttls_available() const { return starttls_available_ && !is_tls(); }
    void set_starttls_available(bool available) { starttls_available_ = available; }

#ifdef ENABLE_TLS
    ssl::context* ssl_context() { return ssl_context_; }
    void set_ssl_context(ssl::context* ctx) { ssl_context_ = ctx; }
#endif

    // AUTH state handling
    void set_auth_username(const std::string& username) { auth_username_ = username; }
    const std::string& auth_username() const { return auth_username_; }

    const std::filesystem::path& maildir_root() const { return maildir_root_; }

protected:
    void on_connect() override;
    void on_data(const std::string& data) override;
    void on_tls_handshake_complete() override;

private:
    void process_command(const std::string& line);
    void process_data_line(const std::string& line);
    void process_auth_response(const std::string& line);

    SessionState state_ = SessionState::CONNECTED;
    AuthState auth_state_ = AuthState::NONE;
    Envelope envelope_;
    std::string client_hostname_;

    std::shared_ptr<Authenticator> auth_;
    std::shared_ptr<SMTPRelay> relay_;
    std::filesystem::path maildir_root_;
    std::string hostname_;
    std::vector<std::string> local_domains_;

    size_t max_message_size_ = 25 * 1024 * 1024;  // 25 MB
    size_t max_recipients_ = 100;
    bool require_auth_ = true;
    bool allow_relay_ = false;

    bool starttls_available_ = true;

#ifdef ENABLE_TLS
    ssl::context* ssl_context_ = nullptr;
#endif

    std::string auth_username_;
    std::string data_buffer_;
};

}  // namespace email::smtp
