#include "storage/maildir.hpp"
#include "logger.hpp"
#include <fstream>
#include <sstream>
#include <random>
#include <chrono>
#include <unistd.h>
#include <algorithm>
#include <regex>

namespace email {

Maildir::Maildir(const std::filesystem::path& root, const std::string& domain,
                 const std::string& username)
    : root_(root)
    , domain_(domain)
    , username_(username)
    , maildir_path_(root / domain / username) {
}

bool Maildir::initialize() {
    try {
        // Create main maildir structure
        if (!ensure_mailbox_dirs(maildir_path_)) {
            return false;
        }

        // Create standard IMAP folders
        create_mailbox("Sent");
        create_mailbox("Drafts");
        create_mailbox("Trash");
        create_mailbox("Junk");

        return true;
    } catch (const std::exception& e) {
        last_error_ = e.what();
        LOG_ERROR(last_error_);
        return false;
    }
}

bool Maildir::exists() const {
    return std::filesystem::exists(maildir_path_ / "cur") &&
           std::filesystem::exists(maildir_path_ / "new") &&
           std::filesystem::exists(maildir_path_ / "tmp");
}

bool Maildir::ensure_mailbox_dirs(const std::filesystem::path& mailbox_path) {
    try {
        std::filesystem::create_directories(mailbox_path / "cur");
        std::filesystem::create_directories(mailbox_path / "new");
        std::filesystem::create_directories(mailbox_path / "tmp");
        return true;
    } catch (const std::exception& e) {
        last_error_ = e.what();
        return false;
    }
}

std::filesystem::path Maildir::get_mailbox_path(const std::string& mailbox) const {
    if (mailbox.empty() || mailbox == "INBOX") {
        return maildir_path_;
    }

    // IMAP folder naming convention: .FolderName
    std::string folder_name = "." + mailbox;
    // Replace hierarchy delimiter
    std::replace(folder_name.begin(), folder_name.end(), '/', '.');
    return maildir_path_ / folder_name;
}

bool Maildir::create_mailbox(const std::string& name) {
    if (name.empty() || name == "INBOX") {
        return true;  // INBOX always exists
    }

    auto path = get_mailbox_path(name);
    return ensure_mailbox_dirs(path);
}

bool Maildir::delete_mailbox(const std::string& name) {
    if (name.empty() || name == "INBOX") {
        last_error_ = "Cannot delete INBOX";
        return false;
    }

    auto path = get_mailbox_path(name);
    try {
        std::filesystem::remove_all(path);
        return true;
    } catch (const std::exception& e) {
        last_error_ = e.what();
        return false;
    }
}

bool Maildir::rename_mailbox(const std::string& old_name, const std::string& new_name) {
    if (old_name.empty() || old_name == "INBOX") {
        last_error_ = "Cannot rename INBOX";
        return false;
    }

    auto old_path = get_mailbox_path(old_name);
    auto new_path = get_mailbox_path(new_name);

    try {
        std::filesystem::rename(old_path, new_path);
        return true;
    } catch (const std::exception& e) {
        last_error_ = e.what();
        return false;
    }
}

std::vector<std::string> Maildir::list_mailboxes(const std::string& pattern) {
    std::vector<std::string> mailboxes;
    mailboxes.push_back("INBOX");

    try {
        for (const auto& entry : std::filesystem::directory_iterator(maildir_path_)) {
            if (entry.is_directory()) {
                std::string name = entry.path().filename().string();
                if (name[0] == '.') {
                    // Convert .FolderName to FolderName
                    name = name.substr(1);
                    std::replace(name.begin(), name.end(), '.', '/');

                    // Check if it's a valid maildir (has cur, new, tmp)
                    auto path = entry.path();
                    if (std::filesystem::exists(path / "cur") &&
                        std::filesystem::exists(path / "new") &&
                        std::filesystem::exists(path / "tmp")) {

                        // Apply pattern matching (simple wildcard)
                        if (pattern == "*" || pattern == "%" ||
                            name.find(pattern.substr(0, pattern.find('*'))) == 0) {
                            mailboxes.push_back(name);
                        }
                    }
                }
            }
        }
    } catch (const std::exception& e) {
        last_error_ = e.what();
    }

    std::sort(mailboxes.begin(), mailboxes.end());
    return mailboxes;
}

std::optional<MailboxInfo> Maildir::get_mailbox_info(const std::string& name) {
    auto path = get_mailbox_path(name);

    if (!std::filesystem::exists(path / "cur")) {
        return std::nullopt;
    }

    MailboxInfo info;
    info.name = name.empty() ? "INBOX" : name;
    info.uid_validity = get_uid_validity(name);
    info.uid_next = allocate_uid(name);

    auto messages = list_messages(name);
    info.total_messages = messages.size();

    for (const auto& msg : messages) {
        info.total_size += msg.size;
        if (msg.is_new) {
            info.recent_messages++;
        }
        if (!msg.is_seen()) {
            info.unseen_messages++;
        }
    }

    info.flags = {"\\Seen", "\\Answered", "\\Flagged", "\\Deleted", "\\Draft"};

    return info;
}

std::string Maildir::generate_unique_name() const {
    auto now = std::chrono::system_clock::now();
    auto duration = now.time_since_epoch();
    auto seconds = std::chrono::duration_cast<std::chrono::seconds>(duration).count();
    auto micros = std::chrono::duration_cast<std::chrono::microseconds>(duration).count() % 1000000;

    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> dis(0, 999999);

    char hostname[256];
    gethostname(hostname, sizeof(hostname));

    std::ostringstream oss;
    oss << seconds << ".M" << micros << "P" << getpid() << "R" << dis(gen) << "." << hostname;

    return oss.str();
}

std::string Maildir::flags_to_info(const std::set<char>& flags) const {
    if (flags.empty()) {
        return "";
    }

    std::string info = ":2,";
    for (char flag : flags) {
        info += flag;
    }
    return info;
}

std::set<char> Maildir::parse_flags(const std::string& filename) const {
    std::set<char> flags;

    auto pos = filename.find(":2,");
    if (pos != std::string::npos) {
        for (size_t i = pos + 3; i < filename.length(); ++i) {
            flags.insert(filename[i]);
        }
    }

    return flags;
}

std::string Maildir::deliver(const std::string& content, const std::string& mailbox) {
    auto path = get_mailbox_path(mailbox);

    if (!std::filesystem::exists(path / "tmp")) {
        if (!ensure_mailbox_dirs(path)) {
            return "";
        }
    }

    std::string unique_name = generate_unique_name();
    auto tmp_path = path / "tmp" / unique_name;
    auto new_path = path / "new" / unique_name;

    try {
        // Write to tmp first (atomic delivery)
        {
            std::ofstream file(tmp_path, std::ios::binary);
            if (!file) {
                last_error_ = "Failed to create temporary file";
                return "";
            }
            file << content;
            file.flush();
            if (!file) {
                last_error_ = "Failed to write message";
                std::filesystem::remove(tmp_path);
                return "";
            }
        }

        // Move to new
        std::filesystem::rename(tmp_path, new_path);

        return unique_name;
    } catch (const std::exception& e) {
        last_error_ = e.what();
        std::filesystem::remove(tmp_path);
        return "";
    }
}

std::optional<Message> Maildir::parse_message_file(const std::filesystem::path& path,
                                                   const std::string& mailbox) const {
    if (!std::filesystem::exists(path)) {
        return std::nullopt;
    }

    Message msg;
    msg.path = path;
    msg.mailbox = mailbox.empty() ? "INBOX" : mailbox;

    std::string filename = path.filename().string();

    // Parse unique_id (everything before :2,)
    auto colon_pos = filename.find(':');
    if (colon_pos != std::string::npos) {
        msg.unique_id = filename.substr(0, colon_pos);
    } else {
        msg.unique_id = filename;
    }

    msg.flags = parse_flags(filename);
    msg.is_new = (path.parent_path().filename() == "new");

    try {
        msg.size = std::filesystem::file_size(path);
        msg.timestamp = std::chrono::system_clock::from_time_t(
            std::chrono::duration_cast<std::chrono::seconds>(
                std::filesystem::last_write_time(path).time_since_epoch()
            ).count()
        );
    } catch (...) {
        // Ignore errors getting file metadata
    }

    return msg;
}

std::optional<Message> Maildir::get_message(const std::string& unique_id,
                                            const std::string& mailbox) {
    auto path = get_mailbox_path(mailbox);

    // Search in cur directory
    for (const auto& entry : std::filesystem::directory_iterator(path / "cur")) {
        std::string filename = entry.path().filename().string();
        if (filename.find(unique_id) == 0) {
            return parse_message_file(entry.path(), mailbox);
        }
    }

    // Search in new directory
    for (const auto& entry : std::filesystem::directory_iterator(path / "new")) {
        std::string filename = entry.path().filename().string();
        if (filename.find(unique_id) == 0) {
            return parse_message_file(entry.path(), mailbox);
        }
    }

    return std::nullopt;
}

std::optional<std::string> Maildir::get_message_content(const std::string& unique_id,
                                                        const std::string& mailbox) {
    auto msg = get_message(unique_id, mailbox);
    if (!msg) {
        return std::nullopt;
    }

    std::ifstream file(msg->path, std::ios::binary);
    if (!file) {
        return std::nullopt;
    }

    std::ostringstream oss;
    oss << file.rdbuf();
    return oss.str();
}

std::optional<std::string> Maildir::get_message_headers(const std::string& unique_id,
                                                        const std::string& mailbox) {
    auto content = get_message_content(unique_id, mailbox);
    if (!content) {
        return std::nullopt;
    }

    // Headers end at the first empty line
    auto pos = content->find("\r\n\r\n");
    if (pos == std::string::npos) {
        pos = content->find("\n\n");
    }

    if (pos != std::string::npos) {
        return content->substr(0, pos);
    }

    return content;
}

std::vector<Message> Maildir::list_messages(const std::string& mailbox) {
    std::vector<Message> messages;
    auto path = get_mailbox_path(mailbox);

    if (!std::filesystem::exists(path)) {
        return messages;
    }

    // List messages in cur
    if (std::filesystem::exists(path / "cur")) {
        for (const auto& entry : std::filesystem::directory_iterator(path / "cur")) {
            if (entry.is_regular_file()) {
                if (auto msg = parse_message_file(entry.path(), mailbox)) {
                    messages.push_back(*msg);
                }
            }
        }
    }

    // List messages in new
    if (std::filesystem::exists(path / "new")) {
        for (const auto& entry : std::filesystem::directory_iterator(path / "new")) {
            if (entry.is_regular_file()) {
                if (auto msg = parse_message_file(entry.path(), mailbox)) {
                    messages.push_back(*msg);
                }
            }
        }
    }

    // Sort by timestamp (oldest first)
    std::sort(messages.begin(), messages.end(),
              [](const Message& a, const Message& b) {
                  return a.timestamp < b.timestamp;
              });

    return messages;
}

std::vector<Message> Maildir::list_new_messages(const std::string& mailbox) {
    std::vector<Message> messages;
    auto path = get_mailbox_path(mailbox);

    if (!std::filesystem::exists(path / "new")) {
        return messages;
    }

    for (const auto& entry : std::filesystem::directory_iterator(path / "new")) {
        if (entry.is_regular_file()) {
            if (auto msg = parse_message_file(entry.path(), mailbox)) {
                messages.push_back(*msg);
            }
        }
    }

    return messages;
}

bool Maildir::move_to_cur(const std::string& unique_id, const std::string& mailbox) {
    auto path = get_mailbox_path(mailbox);
    auto new_dir = path / "new";
    auto cur_dir = path / "cur";

    for (const auto& entry : std::filesystem::directory_iterator(new_dir)) {
        std::string filename = entry.path().filename().string();
        if (filename.find(unique_id) == 0) {
            auto new_filename = filename;
            if (new_filename.find(":2,") == std::string::npos) {
                new_filename += ":2,";
            }
            std::filesystem::rename(entry.path(), cur_dir / new_filename);
            return true;
        }
    }

    return false;
}

bool Maildir::delete_message(const std::string& unique_id, const std::string& mailbox) {
    auto msg = get_message(unique_id, mailbox);
    if (!msg) {
        return false;
    }

    try {
        std::filesystem::remove(msg->path);
        return true;
    } catch (const std::exception& e) {
        last_error_ = e.what();
        return false;
    }
}

bool Maildir::move_message(const std::string& unique_id, const std::string& from_mailbox,
                           const std::string& to_mailbox) {
    auto msg = get_message(unique_id, from_mailbox);
    if (!msg) {
        return false;
    }

    auto dest_path = get_mailbox_path(to_mailbox);
    if (!std::filesystem::exists(dest_path / "cur")) {
        if (!ensure_mailbox_dirs(dest_path)) {
            return false;
        }
    }

    try {
        auto new_path = dest_path / "cur" / msg->path.filename();
        std::filesystem::rename(msg->path, new_path);
        return true;
    } catch (const std::exception& e) {
        last_error_ = e.what();
        return false;
    }
}

bool Maildir::copy_message(const std::string& unique_id, const std::string& from_mailbox,
                           const std::string& to_mailbox) {
    auto content = get_message_content(unique_id, from_mailbox);
    if (!content) {
        return false;
    }

    return !deliver(*content, to_mailbox).empty();
}

bool Maildir::set_flags(const std::string& unique_id, const std::set<char>& flags,
                        const std::string& mailbox) {
    auto msg = get_message(unique_id, mailbox);
    if (!msg) {
        return false;
    }

    // Move from new to cur if necessary
    if (msg->is_new) {
        move_to_cur(unique_id, mailbox);
        msg = get_message(unique_id, mailbox);
        if (!msg) return false;
    }

    std::string new_filename = msg->unique_id + flags_to_info(flags);
    auto new_path = msg->path.parent_path() / new_filename;

    try {
        std::filesystem::rename(msg->path, new_path);
        return true;
    } catch (const std::exception& e) {
        last_error_ = e.what();
        return false;
    }
}

bool Maildir::add_flags(const std::string& unique_id, const std::set<char>& flags,
                        const std::string& mailbox) {
    auto msg = get_message(unique_id, mailbox);
    if (!msg) {
        return false;
    }

    std::set<char> new_flags = msg->flags;
    new_flags.insert(flags.begin(), flags.end());
    return set_flags(unique_id, new_flags, mailbox);
}

bool Maildir::remove_flags(const std::string& unique_id, const std::set<char>& flags,
                           const std::string& mailbox) {
    auto msg = get_message(unique_id, mailbox);
    if (!msg) {
        return false;
    }

    std::set<char> new_flags = msg->flags;
    for (char flag : flags) {
        new_flags.erase(flag);
    }
    return set_flags(unique_id, new_flags, mailbox);
}

bool Maildir::mark_as_seen(const std::string& unique_id, const std::string& mailbox) {
    return add_flags(unique_id, {'S'}, mailbox);
}

size_t Maildir::expunge(const std::string& mailbox) {
    auto messages = list_messages(mailbox);
    size_t count = 0;

    for (const auto& msg : messages) {
        if (msg.is_deleted()) {
            if (delete_message(msg.unique_id, mailbox)) {
                count++;
            }
        }
    }

    return count;
}

size_t Maildir::get_total_size() const {
    size_t total = 0;

    try {
        for (const auto& entry : std::filesystem::recursive_directory_iterator(maildir_path_)) {
            if (entry.is_regular_file()) {
                total += entry.file_size();
            }
        }
    } catch (...) {
        // Ignore errors
    }

    return total;
}

size_t Maildir::get_message_count(const std::string& mailbox) const {
    auto path = get_mailbox_path(mailbox);
    size_t count = 0;

    try {
        if (std::filesystem::exists(path / "cur")) {
            for (const auto& entry : std::filesystem::directory_iterator(path / "cur")) {
                if (entry.is_regular_file()) count++;
            }
        }
        if (std::filesystem::exists(path / "new")) {
            for (const auto& entry : std::filesystem::directory_iterator(path / "new")) {
                if (entry.is_regular_file()) count++;
            }
        }
    } catch (...) {
        // Ignore errors
    }

    return count;
}

uint32_t Maildir::get_uid_validity(const std::string& mailbox) {
    auto path = get_mailbox_path(mailbox);
    auto uidvalidity_file = path / ".uidvalidity";

    if (std::filesystem::exists(uidvalidity_file)) {
        std::ifstream file(uidvalidity_file);
        uint32_t validity;
        if (file >> validity) {
            return validity;
        }
    }

    // Create new UID validity
    auto now = std::chrono::system_clock::now();
    uint32_t validity = static_cast<uint32_t>(
        std::chrono::duration_cast<std::chrono::seconds>(now.time_since_epoch()).count()
    );

    std::ofstream file(uidvalidity_file);
    file << validity << "\n1\n";  // validity and next UID
    return validity;
}

uint32_t Maildir::allocate_uid(const std::string& mailbox) {
    auto path = get_mailbox_path(mailbox);
    auto uidvalidity_file = path / ".uidvalidity";

    uint32_t validity = 0;
    uint32_t next_uid = 1;

    if (std::filesystem::exists(uidvalidity_file)) {
        std::ifstream file(uidvalidity_file);
        file >> validity >> next_uid;
    }

    // Write back incremented UID
    std::ofstream file(uidvalidity_file);
    file << validity << "\n" << (next_uid + 1) << "\n";

    return next_uid;
}

}  // namespace email
