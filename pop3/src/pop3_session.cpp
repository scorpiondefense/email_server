#include "pop3_session.hpp"
#include "logger.hpp"
#include <sstream>

namespace email::pop3 {

POP3Session::POP3Session(asio::io_context& io_context, tcp::socket socket,
                         std::shared_ptr<Authenticator> auth,
                         const std::filesystem::path& maildir_root,
                         const std::string& hostname)
    : Session(io_context, std::move(socket))
    , auth_(std::move(auth))
    , maildir_root_(maildir_root)
    , hostname_(hostname) {
}

#ifdef ENABLE_TLS
POP3Session::POP3Session(asio::io_context& io_context, tcp::socket socket,
                         ssl::context& ssl_ctx,
                         std::shared_ptr<Authenticator> auth,
                         const std::filesystem::path& maildir_root,
                         const std::string& hostname)
    : Session(io_context, std::move(socket), ssl_ctx)
    , auth_(std::move(auth))
    , maildir_root_(maildir_root)
    , hostname_(hostname)
    , starttls_available_(false) {  // Already TLS, no STARTTLS needed
}
#endif

void POP3Session::on_connect() {
    Session::on_connect();
    LOG_INFO_FMT("POP3 connection from {}:{}", remote_address(), remote_port());

    // Send greeting
    send_line(response::ok(hostname_ + " POP3 server ready"));
}

void POP3Session::on_data(const std::string& data) {
    process_command(data);
}

void POP3Session::on_tls_handshake_complete() {
    Session::on_tls_handshake_complete();
    LOG_INFO("POP3 TLS handshake completed");
}

void POP3Session::process_command(const std::string& line) {
    LOG_DEBUG_FMT("POP3 command: {}", line);

    Command cmd = Command::parse(line);

    if (cmd.type == CommandType::UNKNOWN) {
        send_line(response::err("Unknown command"));
        return;
    }

    std::string response = CommandHandler::instance().execute(*this, cmd);

    if (!response.empty()) {
        // Check if it's a multi-line response
        if (response.find("\r\n") != std::string::npos) {
            send(response + "\r\n");
        } else {
            send_line(response);
        }
    }

    // Handle QUIT
    if (cmd.type == CommandType::QUIT) {
        stop();
    }
}

bool POP3Session::open_maildir() {
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

    load_messages();
    return true;
}

void POP3Session::load_messages() {
    messages_.clear();
    deleted_messages_.clear();

    if (!maildir_) return;

    auto msgs = maildir_->list_messages("INBOX");
    size_t number = 1;

    for (const auto& msg : msgs) {
        MessageInfo info;
        info.number = number++;
        info.unique_id = msg.unique_id;
        info.size = msg.size;
        info.deleted = false;
        messages_.push_back(info);
    }

    LOG_DEBUG_FMT("Loaded {} messages for {}@{}", messages_.size(), username(), domain());
}

std::optional<MessageInfo> POP3Session::get_message(size_t number) const {
    if (number < 1 || number > messages_.size()) {
        return std::nullopt;
    }

    MessageInfo info = messages_[number - 1];
    info.deleted = deleted_messages_.count(number) > 0;
    return info;
}

std::optional<std::string> POP3Session::get_message_content(size_t number) const {
    auto msg = get_message(number);
    if (!msg || !maildir_) {
        return std::nullopt;
    }

    return maildir_->get_message_content(msg->unique_id, "INBOX");
}

std::optional<std::string> POP3Session::get_message_top(size_t number, size_t lines) const {
    auto content = get_message_content(number);
    if (!content) {
        return std::nullopt;
    }

    // Find end of headers
    auto header_end = content->find("\r\n\r\n");
    if (header_end == std::string::npos) {
        header_end = content->find("\n\n");
        if (header_end == std::string::npos) {
            return content;  // No body
        }
    }

    std::string result = content->substr(0, header_end + 2);  // Include blank line

    if (lines == 0) {
        return result;
    }

    // Add requested body lines
    std::string body = content->substr(header_end + 4);  // Skip \r\n\r\n
    std::istringstream iss(body);
    std::string line;
    size_t count = 0;

    result += "\r\n";  // Blank line between headers and body

    while (std::getline(iss, line) && count < lines) {
        if (!line.empty() && line.back() == '\r') {
            line.pop_back();
        }
        result += line + "\r\n";
        count++;
    }

    return result;
}

bool POP3Session::mark_deleted(size_t number) {
    if (number < 1 || number > messages_.size()) {
        return false;
    }

    deleted_messages_.insert(number);
    return true;
}

void POP3Session::reset_deleted() {
    deleted_messages_.clear();
}

size_t POP3Session::expunge() {
    if (!maildir_) return 0;

    size_t count = 0;
    for (size_t num : deleted_messages_) {
        if (num <= messages_.size()) {
            if (maildir_->delete_message(messages_[num - 1].unique_id, "INBOX")) {
                count++;
            }
        }
    }

    return count;
}

size_t POP3Session::total_messages() const {
    size_t count = 0;
    for (size_t i = 0; i < messages_.size(); ++i) {
        if (deleted_messages_.count(i + 1) == 0) {
            count++;
        }
    }
    return count;
}

size_t POP3Session::total_size() const {
    size_t size = 0;
    for (size_t i = 0; i < messages_.size(); ++i) {
        if (deleted_messages_.count(i + 1) == 0) {
            size += messages_[i].size;
        }
    }
    return size;
}

}  // namespace email::pop3
