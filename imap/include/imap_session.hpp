#pragma once

#include "net/session.hpp"
#include "auth/authenticator.hpp"
#include "storage/maildir.hpp"
#include "imap_commands.hpp"
#include "imap_parser.hpp"
#include <memory>
#include <vector>
#include <set>
#include <map>

namespace email::imap {

enum class SessionState {
    NOT_AUTHENTICATED,
    AUTHENTICATED,
    SELECTED,
    LOGOUT
};

struct SelectedMailbox {
    std::string name;
    bool read_only = false;
    uint32_t uid_validity = 0;
    uint32_t uid_next = 0;
    size_t exists = 0;
    size_t recent = 0;
    size_t unseen = 0;
    std::vector<std::string> flags;
    std::vector<std::string> permanent_flags;
};

struct CachedMessage {
    uint32_t sequence_number;
    uint32_t uid;
    std::string unique_id;
    size_t size;
    std::set<std::string> flags;
    std::chrono::system_clock::time_point internal_date;
};

class IMAPSession : public Session {
public:
    IMAPSession(asio::io_context& io_context, tcp::socket socket,
                std::shared_ptr<Authenticator> auth,
                const std::filesystem::path& maildir_root,
                const std::string& hostname);

#ifdef ENABLE_TLS
    IMAPSession(asio::io_context& io_context, tcp::socket socket,
                ssl::context& ssl_ctx,
                std::shared_ptr<Authenticator> auth,
                const std::filesystem::path& maildir_root,
                const std::string& hostname);
#endif

    ~IMAPSession() override = default;

    // State
    SessionState state() const { return state_; }
    void set_state(SessionState state) { state_ = state; }

    // Authentication
    Authenticator& authenticator() { return *auth_; }

    // Maildir
    Maildir* maildir() { return maildir_.get(); }
    bool open_maildir();

    // Mailbox operations
    bool select_mailbox(const std::string& name, bool read_only = false);
    void close_mailbox();
    const SelectedMailbox* selected_mailbox() const {
        return selected_ ? &*selected_ : nullptr;
    }

    std::vector<std::string> list_mailboxes(const std::string& reference,
                                            const std::string& pattern);

    // Message operations
    const std::vector<CachedMessage>& messages() const { return messages_; }
    std::optional<CachedMessage> get_message_by_sequence(uint32_t seq) const;
    std::optional<CachedMessage> get_message_by_uid(uint32_t uid) const;
    std::optional<std::string> get_message_content(uint32_t seq) const;
    std::optional<std::string> get_message_headers(uint32_t seq) const;

    // Flag operations
    bool set_flags(uint32_t seq, const std::set<std::string>& flags);
    bool add_flags(uint32_t seq, const std::set<std::string>& flags);
    bool remove_flags(uint32_t seq, const std::set<std::string>& flags);

    // Search
    std::vector<uint32_t> search(const std::vector<SearchCriteria>& criteria, bool use_uid = false);

    // Expunge
    std::vector<uint32_t> expunge();

    // STARTTLS
    bool starttls_available() const { return starttls_available_ && !is_tls(); }
    void set_starttls_available(bool available) { starttls_available_ = available; }

#ifdef ENABLE_TLS
    ssl::context* ssl_context() { return ssl_context_; }
    void set_ssl_context(ssl::context* ctx) { ssl_context_ = ctx; }
#endif

    const std::string& hostname() const { return hostname_; }

    // UID handling
    uint32_t get_uid_for_sequence(uint32_t seq) const;
    uint32_t get_sequence_for_uid(uint32_t uid) const;

protected:
    void on_connect() override;
    void on_data(const std::string& data) override;
    void on_tls_handshake_complete() override;

private:
    void process_command(const std::string& line);
    void load_messages();
    void update_mailbox_counts();

    // Convert maildir flags to IMAP flags
    static std::set<std::string> maildir_to_imap_flags(const std::set<char>& flags);
    static std::set<char> imap_to_maildir_flags(const std::set<std::string>& flags);

    SessionState state_ = SessionState::NOT_AUTHENTICATED;

    std::shared_ptr<Authenticator> auth_;
    std::filesystem::path maildir_root_;
    std::unique_ptr<Maildir> maildir_;
    std::string hostname_;

    std::optional<SelectedMailbox> selected_;
    std::vector<CachedMessage> messages_;

    bool starttls_available_ = true;

#ifdef ENABLE_TLS
    ssl::context* ssl_context_ = nullptr;
#endif

    // UID mapping
    std::map<uint32_t, uint32_t> seq_to_uid_;
    std::map<uint32_t, uint32_t> uid_to_seq_;
    uint32_t next_uid_ = 1;
};

}  // namespace email::imap
