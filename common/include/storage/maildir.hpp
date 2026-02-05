#pragma once

#include <string>
#include <filesystem>
#include <vector>
#include <optional>
#include <chrono>
#include <set>
#include <cstdint>

namespace email {

struct Message {
    std::string unique_id;     // Unique identifier (filename base)
    std::filesystem::path path;
    size_t size = 0;
    std::chrono::system_clock::time_point timestamp;
    std::set<char> flags;      // IMAP flags: S=seen, R=replied, F=flagged, T=trashed, D=draft
    bool is_new = true;        // In 'new' vs 'cur' directory
    std::string mailbox;       // Mailbox name (INBOX, Sent, etc.)

    bool has_flag(char flag) const { return flags.count(flag) > 0; }
    void add_flag(char flag) { flags.insert(flag); }
    void remove_flag(char flag) { flags.erase(flag); }

    // IMAP flag names
    bool is_seen() const { return has_flag('S'); }
    bool is_answered() const { return has_flag('R'); }
    bool is_flagged() const { return has_flag('F'); }
    bool is_deleted() const { return has_flag('T'); }
    bool is_draft() const { return has_flag('D'); }
};

struct MailboxInfo {
    std::string name;
    size_t total_messages = 0;
    size_t recent_messages = 0;
    size_t unseen_messages = 0;
    size_t total_size = 0;
    uint32_t uid_validity = 0;
    uint32_t uid_next = 0;
    bool is_selectable = true;
    bool has_children = false;
    std::vector<std::string> flags;
};

class Maildir {
public:
    Maildir(const std::filesystem::path& root, const std::string& domain,
            const std::string& username);

    bool initialize();
    bool exists() const;
    std::filesystem::path path() const { return maildir_path_; }

    // Mailbox operations
    bool create_mailbox(const std::string& name);
    bool delete_mailbox(const std::string& name);
    bool rename_mailbox(const std::string& old_name, const std::string& new_name);
    std::vector<std::string> list_mailboxes(const std::string& pattern = "*");
    std::optional<MailboxInfo> get_mailbox_info(const std::string& name);

    // Message operations
    std::string deliver(const std::string& content, const std::string& mailbox = "INBOX");
    std::optional<Message> get_message(const std::string& unique_id,
                                       const std::string& mailbox = "INBOX");
    std::optional<std::string> get_message_content(const std::string& unique_id,
                                                   const std::string& mailbox = "INBOX");
    std::optional<std::string> get_message_headers(const std::string& unique_id,
                                                   const std::string& mailbox = "INBOX");
    std::vector<Message> list_messages(const std::string& mailbox = "INBOX");
    std::vector<Message> list_new_messages(const std::string& mailbox = "INBOX");

    // Message manipulation
    bool delete_message(const std::string& unique_id, const std::string& mailbox = "INBOX");
    bool move_message(const std::string& unique_id, const std::string& from_mailbox,
                      const std::string& to_mailbox);
    bool copy_message(const std::string& unique_id, const std::string& from_mailbox,
                      const std::string& to_mailbox);
    bool set_flags(const std::string& unique_id, const std::set<char>& flags,
                   const std::string& mailbox = "INBOX");
    bool add_flags(const std::string& unique_id, const std::set<char>& flags,
                   const std::string& mailbox = "INBOX");
    bool remove_flags(const std::string& unique_id, const std::set<char>& flags,
                      const std::string& mailbox = "INBOX");

    // POP3 operations
    bool mark_as_seen(const std::string& unique_id, const std::string& mailbox = "INBOX");
    size_t expunge(const std::string& mailbox = "INBOX");  // Remove deleted messages

    // Storage info
    size_t get_total_size() const;
    size_t get_message_count(const std::string& mailbox = "INBOX") const;

    std::string last_error() const { return last_error_; }

    // UID management for IMAP
    uint32_t get_uid_validity(const std::string& mailbox = "INBOX");
    uint32_t allocate_uid(const std::string& mailbox = "INBOX");

private:
    std::filesystem::path get_mailbox_path(const std::string& mailbox) const;
    std::string generate_unique_name() const;
    std::string flags_to_info(const std::set<char>& flags) const;
    std::set<char> parse_flags(const std::string& filename) const;
    std::optional<Message> parse_message_file(const std::filesystem::path& path,
                                              const std::string& mailbox) const;
    bool ensure_mailbox_dirs(const std::filesystem::path& mailbox_path);
    bool move_to_cur(const std::string& unique_id, const std::string& mailbox);

    std::filesystem::path root_;
    std::string domain_;
    std::string username_;
    std::filesystem::path maildir_path_;
    std::string last_error_;
};

}  // namespace email
