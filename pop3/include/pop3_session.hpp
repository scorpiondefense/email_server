#pragma once

#include "net/session.hpp"
#include "auth/authenticator.hpp"
#include "storage/maildir.hpp"
#include "pop3_commands.hpp"
#include <memory>
#include <vector>
#include <set>

namespace email::pop3 {

enum class SessionState {
    AUTHORIZATION,  // Before authentication
    TRANSACTION,    // After authentication
    UPDATE          // During QUIT processing
};

struct MessageInfo {
    size_t number;           // 1-based message number
    std::string unique_id;   // Unique identifier
    size_t size;             // Size in bytes
    bool deleted;            // Marked for deletion
};

class POP3Session : public Session {
public:
    POP3Session(asio::io_context& io_context, tcp::socket socket,
                std::shared_ptr<Authenticator> auth,
                const std::filesystem::path& maildir_root,
                const std::string& hostname);

#ifdef ENABLE_TLS
    POP3Session(asio::io_context& io_context, tcp::socket socket,
                ssl::context& ssl_ctx,
                std::shared_ptr<Authenticator> auth,
                const std::filesystem::path& maildir_root,
                const std::string& hostname);
#endif

    ~POP3Session() override = default;

    // State access
    SessionState state() const { return state_; }
    void set_state(SessionState state) { state_ = state; }

    const std::string& pending_username() const { return pending_username_; }
    void set_pending_username(const std::string& username) { pending_username_ = username; }

    // Authenticator access
    Authenticator& authenticator() { return *auth_; }

    // Maildir access
    Maildir* maildir() { return maildir_.get(); }
    bool open_maildir();

    // Message operations
    const std::vector<MessageInfo>& messages() const { return messages_; }
    std::optional<MessageInfo> get_message(size_t number) const;
    std::optional<std::string> get_message_content(size_t number) const;
    std::optional<std::string> get_message_top(size_t number, size_t lines) const;

    bool mark_deleted(size_t number);
    void reset_deleted();
    size_t expunge();  // Actually delete marked messages

    // Statistics
    size_t total_messages() const;
    size_t total_size() const;

    // STLS support
    bool starttls_available() const { return starttls_available_ && !is_tls(); }
    void set_starttls_available(bool available) { starttls_available_ = available; }

#ifdef ENABLE_TLS
    ssl::context* ssl_context() { return ssl_context_; }
    void set_ssl_context(ssl::context* ctx) { ssl_context_ = ctx; }
#endif

    const std::string& hostname() const { return hostname_; }

protected:
    void on_connect() override;
    void on_data(const std::string& data) override;
    void on_tls_handshake_complete() override;

private:
    void process_command(const std::string& line);
    void load_messages();

    SessionState state_ = SessionState::AUTHORIZATION;
    std::string pending_username_;

    std::shared_ptr<Authenticator> auth_;
    std::filesystem::path maildir_root_;
    std::unique_ptr<Maildir> maildir_;
    std::string hostname_;

    std::vector<MessageInfo> messages_;
    std::set<size_t> deleted_messages_;

    bool starttls_available_ = true;

#ifdef ENABLE_TLS
    ssl::context* ssl_context_ = nullptr;
#endif
};

}  // namespace email::pop3
