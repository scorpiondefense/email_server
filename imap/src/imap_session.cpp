#include "imap_session.hpp"
#include "logger.hpp"
#include <sstream>
#include <algorithm>

namespace email::imap {

IMAPSession::IMAPSession(asio::io_context& io_context, tcp::socket socket,
                         std::shared_ptr<Authenticator> auth,
                         const std::filesystem::path& maildir_root,
                         const std::string& hostname)
    : Session(io_context, std::move(socket))
    , auth_(std::move(auth))
    , maildir_root_(maildir_root)
    , hostname_(hostname) {
}

#ifdef ENABLE_TLS
IMAPSession::IMAPSession(asio::io_context& io_context, tcp::socket socket,
                         ssl::context& ssl_ctx,
                         std::shared_ptr<Authenticator> auth,
                         const std::filesystem::path& maildir_root,
                         const std::string& hostname)
    : Session(io_context, std::move(socket), ssl_ctx)
    , auth_(std::move(auth))
    , maildir_root_(maildir_root)
    , hostname_(hostname)
    , starttls_available_(false) {
}
#endif

void IMAPSession::on_connect() {
    Session::on_connect();
    LOG_INFO_FMT("IMAP connection from {}:{}", remote_address(), remote_port());

    // Send greeting
    send_line("* OK " + hostname_ + " IMAP4rev1 Service Ready");
}

void IMAPSession::on_data(const std::string& data) {
    process_command(data);
}

void IMAPSession::on_tls_handshake_complete() {
    Session::on_tls_handshake_complete();
    LOG_INFO("IMAP TLS handshake completed");
}

void IMAPSession::process_command(const std::string& line) {
    LOG_DEBUG_FMT("IMAP command: {}", line);

    Command cmd = Command::parse(line);

    if (cmd.type == CommandType::UNKNOWN) {
        send_line(response::bad(cmd.tag.empty() ? "*" : cmd.tag, "Unknown command"));
        return;
    }

    auto responses = CommandHandler::instance().execute(*this, cmd);

    for (const auto& response : responses) {
        send_line(response);
    }

    // Handle LOGOUT
    if (cmd.type == CommandType::LOGOUT) {
        stop();
    }
}

bool IMAPSession::open_maildir() {
    if (username().empty() || domain().empty()) {
        return false;
    }

    maildir_ = std::make_unique<Maildir>(maildir_root_, domain(), username());

    if (!maildir_->exists()) {
        if (!maildir_->initialize()) {
            LOG_ERROR_FMT("Failed to initialize maildir for {}@{}", username(), domain());
            return false;
        }
    }

    return true;
}

bool IMAPSession::select_mailbox(const std::string& name, bool read_only) {
    if (!maildir_) return false;

    std::string mailbox_name = name;
    if (mailbox_name.empty()) mailbox_name = "INBOX";

    auto info = maildir_->get_mailbox_info(mailbox_name);
    if (!info) {
        return false;
    }

    selected_ = SelectedMailbox{};
    selected_->name = mailbox_name;
    selected_->read_only = read_only;
    selected_->uid_validity = info->uid_validity;
    selected_->uid_next = info->uid_next;
    selected_->exists = info->total_messages;
    selected_->recent = info->recent_messages;
    selected_->unseen = info->unseen_messages;
    selected_->flags = {"\\Answered", "\\Flagged", "\\Deleted", "\\Seen", "\\Draft"};
    selected_->permanent_flags = selected_->flags;
    selected_->permanent_flags.push_back("\\*");

    load_messages();
    set_state(SessionState::SELECTED);

    return true;
}

void IMAPSession::close_mailbox() {
    selected_.reset();
    messages_.clear();
    seq_to_uid_.clear();
    uid_to_seq_.clear();
    set_state(SessionState::AUTHENTICATED);
}

std::vector<std::string> IMAPSession::list_mailboxes(const std::string& /* reference */,
                                                     const std::string& pattern) {
    if (!maildir_) return {};
    return maildir_->list_mailboxes(pattern);
}

void IMAPSession::load_messages() {
    messages_.clear();
    seq_to_uid_.clear();
    uid_to_seq_.clear();

    if (!maildir_ || !selected_) return;

    auto msgs = maildir_->list_messages(selected_->name);
    uint32_t seq = 1;

    for (const auto& msg : msgs) {
        CachedMessage cached;
        cached.sequence_number = seq;
        cached.uid = next_uid_++;
        cached.unique_id = msg.unique_id;
        cached.size = msg.size;
        cached.flags = maildir_to_imap_flags(msg.flags);
        cached.internal_date = msg.timestamp;

        if (msg.is_new) {
            cached.flags.insert("\\Recent");
        }

        seq_to_uid_[seq] = cached.uid;
        uid_to_seq_[cached.uid] = seq;

        messages_.push_back(cached);
        seq++;
    }

    update_mailbox_counts();
}

void IMAPSession::update_mailbox_counts() {
    if (!selected_) return;

    selected_->exists = messages_.size();
    selected_->recent = 0;
    selected_->unseen = 0;

    for (const auto& msg : messages_) {
        if (msg.flags.count("\\Recent")) {
            selected_->recent++;
        }
        if (!msg.flags.count("\\Seen")) {
            selected_->unseen++;
        }
    }
}

std::optional<CachedMessage> IMAPSession::get_message_by_sequence(uint32_t seq) const {
    if (seq < 1 || seq > messages_.size()) {
        return std::nullopt;
    }
    return messages_[seq - 1];
}

std::optional<CachedMessage> IMAPSession::get_message_by_uid(uint32_t uid) const {
    auto it = uid_to_seq_.find(uid);
    if (it == uid_to_seq_.end()) {
        return std::nullopt;
    }
    return get_message_by_sequence(it->second);
}

std::optional<std::string> IMAPSession::get_message_content(uint32_t seq) const {
    auto msg = get_message_by_sequence(seq);
    if (!msg || !maildir_ || !selected_) {
        return std::nullopt;
    }
    return maildir_->get_message_content(msg->unique_id, selected_->name);
}

std::optional<std::string> IMAPSession::get_message_headers(uint32_t seq) const {
    auto msg = get_message_by_sequence(seq);
    if (!msg || !maildir_ || !selected_) {
        return std::nullopt;
    }
    return maildir_->get_message_headers(msg->unique_id, selected_->name);
}

bool IMAPSession::set_flags(uint32_t seq, const std::set<std::string>& flags) {
    if (seq < 1 || seq > messages_.size() || !maildir_ || !selected_) {
        return false;
    }

    auto maildir_flags = imap_to_maildir_flags(flags);
    if (maildir_->set_flags(messages_[seq - 1].unique_id, maildir_flags, selected_->name)) {
        messages_[seq - 1].flags = flags;
        return true;
    }
    return false;
}

bool IMAPSession::add_flags(uint32_t seq, const std::set<std::string>& flags) {
    if (seq < 1 || seq > messages_.size()) {
        return false;
    }

    auto new_flags = messages_[seq - 1].flags;
    new_flags.insert(flags.begin(), flags.end());
    return set_flags(seq, new_flags);
}

bool IMAPSession::remove_flags(uint32_t seq, const std::set<std::string>& flags) {
    if (seq < 1 || seq > messages_.size()) {
        return false;
    }

    auto new_flags = messages_[seq - 1].flags;
    for (const auto& flag : flags) {
        new_flags.erase(flag);
    }
    return set_flags(seq, new_flags);
}

std::vector<uint32_t> IMAPSession::search(const std::vector<SearchCriteria>& criteria, bool use_uid) {
    std::vector<uint32_t> results;

    for (const auto& msg : messages_) {
        bool matches = true;

        for (const auto& crit : criteria) {
            switch (crit.type) {
                case SearchCriteria::Type::ALL:
                    // Always matches
                    break;
                case SearchCriteria::Type::SEEN:
                    matches = msg.flags.count("\\Seen") > 0;
                    break;
                case SearchCriteria::Type::UNSEEN:
                    matches = msg.flags.count("\\Seen") == 0;
                    break;
                case SearchCriteria::Type::ANSWERED:
                    matches = msg.flags.count("\\Answered") > 0;
                    break;
                case SearchCriteria::Type::UNANSWERED:
                    matches = msg.flags.count("\\Answered") == 0;
                    break;
                case SearchCriteria::Type::DELETED:
                    matches = msg.flags.count("\\Deleted") > 0;
                    break;
                case SearchCriteria::Type::UNDELETED:
                    matches = msg.flags.count("\\Deleted") == 0;
                    break;
                case SearchCriteria::Type::FLAGGED:
                    matches = msg.flags.count("\\Flagged") > 0;
                    break;
                case SearchCriteria::Type::UNFLAGGED:
                    matches = msg.flags.count("\\Flagged") == 0;
                    break;
                case SearchCriteria::Type::DRAFT:
                    matches = msg.flags.count("\\Draft") > 0;
                    break;
                case SearchCriteria::Type::UNDRAFT:
                    matches = msg.flags.count("\\Draft") == 0;
                    break;
                case SearchCriteria::Type::RECENT:
                    matches = msg.flags.count("\\Recent") > 0;
                    break;
                case SearchCriteria::Type::NEW:
                    matches = msg.flags.count("\\Recent") > 0 && msg.flags.count("\\Seen") == 0;
                    break;
                case SearchCriteria::Type::OLD:
                    matches = msg.flags.count("\\Recent") == 0;
                    break;
                case SearchCriteria::Type::LARGER:
                    matches = msg.size > std::stoul(crit.value);
                    break;
                case SearchCriteria::Type::SMALLER:
                    matches = msg.size < std::stoul(crit.value);
                    break;
                case SearchCriteria::Type::UID: {
                    auto set = SequenceSet::parse(crit.value);
                    matches = set.contains(msg.uid);
                    break;
                }
                default:
                    // For complex criteria like BODY, FROM, etc.
                    // Would need to read and parse message content
                    break;
            }

            if (!matches) break;
        }

        if (matches) {
            results.push_back(use_uid ? msg.uid : msg.sequence_number);
        }
    }

    return results;
}

std::vector<uint32_t> IMAPSession::expunge() {
    std::vector<uint32_t> deleted_seqs;

    if (!maildir_ || !selected_) {
        return deleted_seqs;
    }

    // Collect messages to delete
    std::vector<size_t> to_delete;
    for (size_t i = 0; i < messages_.size(); ++i) {
        if (messages_[i].flags.count("\\Deleted")) {
            to_delete.push_back(i);
        }
    }

    // Delete in reverse order to maintain correct sequence numbers
    for (auto it = to_delete.rbegin(); it != to_delete.rend(); ++it) {
        size_t idx = *it;
        maildir_->delete_message(messages_[idx].unique_id, selected_->name);
        deleted_seqs.push_back(static_cast<uint32_t>(idx + 1));
    }

    // Reload messages
    load_messages();

    // Return in correct order (lowest first)
    std::reverse(deleted_seqs.begin(), deleted_seqs.end());
    return deleted_seqs;
}

uint32_t IMAPSession::get_uid_for_sequence(uint32_t seq) const {
    auto it = seq_to_uid_.find(seq);
    return it != seq_to_uid_.end() ? it->second : 0;
}

uint32_t IMAPSession::get_sequence_for_uid(uint32_t uid) const {
    auto it = uid_to_seq_.find(uid);
    return it != uid_to_seq_.end() ? it->second : 0;
}

std::set<std::string> IMAPSession::maildir_to_imap_flags(const std::set<char>& flags) {
    std::set<std::string> imap_flags;

    for (char flag : flags) {
        switch (flag) {
            case 'S': imap_flags.insert("\\Seen"); break;
            case 'R': imap_flags.insert("\\Answered"); break;
            case 'F': imap_flags.insert("\\Flagged"); break;
            case 'T': imap_flags.insert("\\Deleted"); break;
            case 'D': imap_flags.insert("\\Draft"); break;
        }
    }

    return imap_flags;
}

std::set<char> IMAPSession::imap_to_maildir_flags(const std::set<std::string>& flags) {
    std::set<char> maildir_flags;

    for (const auto& flag : flags) {
        if (flag == "\\Seen") maildir_flags.insert('S');
        else if (flag == "\\Answered") maildir_flags.insert('R');
        else if (flag == "\\Flagged") maildir_flags.insert('F');
        else if (flag == "\\Deleted") maildir_flags.insert('T');
        else if (flag == "\\Draft") maildir_flags.insert('D');
    }

    return maildir_flags;
}

}  // namespace email::imap
