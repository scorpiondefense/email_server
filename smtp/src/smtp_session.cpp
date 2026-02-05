#include "smtp_session.hpp"
#include "logger.hpp"
#include <sstream>
#include <chrono>
#include <iomanip>

// Base64 helpers
#include <openssl/bio.h>
#include <openssl/evp.h>
#include <openssl/buffer.h>

namespace email::smtp {

namespace {

std::string base64_decode(const std::string& encoded) {
    if (encoded.empty()) return "";

    BIO* bio = BIO_new_mem_buf(encoded.data(), static_cast<int>(encoded.size()));
    BIO* b64 = BIO_new(BIO_f_base64());
    BIO_set_flags(b64, BIO_FLAGS_BASE64_NO_NL);
    bio = BIO_push(b64, bio);

    std::vector<char> decoded(encoded.size());
    int len = BIO_read(bio, decoded.data(), static_cast<int>(decoded.size()));
    BIO_free_all(bio);

    if (len > 0) {
        return std::string(decoded.data(), static_cast<size_t>(len));
    }
    return "";
}

std::string base64_encode(const std::string& data) {
    BIO* bio = BIO_new(BIO_s_mem());
    BIO* b64 = BIO_new(BIO_f_base64());
    BIO_set_flags(b64, BIO_FLAGS_BASE64_NO_NL);
    bio = BIO_push(b64, bio);

    BIO_write(bio, data.data(), static_cast<int>(data.size()));
    BIO_flush(bio);

    BUF_MEM* buffer;
    BIO_get_mem_ptr(bio, &buffer);

    std::string result(buffer->data, buffer->length);
    BIO_free_all(bio);

    return result;
}

std::string get_timestamp() {
    auto now = std::chrono::system_clock::now();
    auto time = std::chrono::system_clock::to_time_t(now);
    std::tm tm;
#ifdef _WIN32
    localtime_s(&tm, &time);
#else
    localtime_r(&time, &tm);
#endif

    std::ostringstream oss;
    oss << std::put_time(&tm, "%a, %d %b %Y %H:%M:%S %z");
    return oss.str();
}

}  // namespace

SMTPSession::SMTPSession(asio::io_context& io_context, tcp::socket socket,
                         std::shared_ptr<Authenticator> auth,
                         std::shared_ptr<SMTPRelay> relay,
                         const std::filesystem::path& maildir_root,
                         const std::string& hostname,
                         const std::vector<std::string>& local_domains)
    : Session(io_context, std::move(socket))
    , auth_(std::move(auth))
    , relay_(std::move(relay))
    , maildir_root_(maildir_root)
    , hostname_(hostname)
    , local_domains_(local_domains) {
}

#ifdef ENABLE_TLS
SMTPSession::SMTPSession(asio::io_context& io_context, tcp::socket socket,
                         ssl::context& ssl_ctx,
                         std::shared_ptr<Authenticator> auth,
                         std::shared_ptr<SMTPRelay> relay,
                         const std::filesystem::path& maildir_root,
                         const std::string& hostname,
                         const std::vector<std::string>& local_domains)
    : Session(io_context, std::move(socket), ssl_ctx)
    , auth_(std::move(auth))
    , relay_(std::move(relay))
    , maildir_root_(maildir_root)
    , hostname_(hostname)
    , local_domains_(local_domains)
    , starttls_available_(false) {
}
#endif

void SMTPSession::on_connect() {
    Session::on_connect();
    LOG_INFO_FMT("SMTP connection from {}:{}", remote_address(), remote_port());

    // Send greeting
    send_line(reply::make(reply::SERVICE_READY, hostname_ + " ESMTP ready"));
}

void SMTPSession::on_data(const std::string& data) {
    if (state_ == SessionState::DATA) {
        process_data_line(data);
    } else if (auth_state_ == AuthState::PLAIN_WAITING ||
               auth_state_ == AuthState::LOGIN_WAITING_USERNAME ||
               auth_state_ == AuthState::LOGIN_WAITING_PASSWORD) {
        process_auth_response(data);
    } else {
        process_command(data);
    }
}

void SMTPSession::on_tls_handshake_complete() {
    Session::on_tls_handshake_complete();
    LOG_INFO("SMTP TLS handshake completed");
}

void SMTPSession::process_command(const std::string& line) {
    LOG_DEBUG_FMT("SMTP command: {}", line);

    Command cmd = Command::parse(line);

    if (cmd.type == CommandType::UNKNOWN) {
        send_line(reply::make(reply::SYNTAX_ERROR, "Unrecognized command"));
        return;
    }

    std::string response = CommandHandler::instance().execute(*this, cmd);

    if (!response.empty()) {
        send_line(response);
    }

    // Handle QUIT
    if (cmd.type == CommandType::QUIT) {
        stop();
    }
}

void SMTPSession::process_data_line(const std::string& line) {
    // Check for end of data
    if (line == ".") {
        // Deliver message
        if (deliver_message()) {
            send_line(reply::make(reply::OK, "Message accepted for delivery"));
        } else {
            send_line(reply::make(reply::LOCAL_ERROR, "Delivery failed"));
        }

        envelope_.clear();
        state_ = SessionState::GREETED;
        data_buffer_.clear();
        return;
    }

    // Handle byte-stuffing (lines starting with . have extra . prepended)
    std::string content_line = line;
    if (!content_line.empty() && content_line[0] == '.') {
        content_line = content_line.substr(1);
    }

    // Check message size
    if (data_buffer_.size() + content_line.size() + 2 > max_message_size_) {
        send_line(reply::make(reply::EXCEEDED_STORAGE, "Message too large"));
        envelope_.clear();
        state_ = SessionState::GREETED;
        data_buffer_.clear();
        return;
    }

    data_buffer_ += content_line + "\r\n";
}

void SMTPSession::process_auth_response(const std::string& line) {
    std::string decoded = base64_decode(line);

    switch (auth_state_) {
        case AuthState::PLAIN_WAITING: {
            // PLAIN format: \0username\0password
            size_t first_null = decoded.find('\0');
            size_t second_null = decoded.find('\0', first_null + 1);

            if (first_null == std::string::npos || second_null == std::string::npos) {
                send_line(reply::make(reply::AUTH_INVALID, "Invalid credentials format"));
                auth_state_ = AuthState::NONE;
                return;
            }

            std::string username = decoded.substr(first_null + 1, second_null - first_null - 1);
            std::string password = decoded.substr(second_null + 1);

            if (auth_->authenticate(username, password)) {
                set_authenticated(true);
                auto [user, domain] = Authenticator::parse_email(username);
                set_username(user);
                set_domain(domain);
                auth_state_ = AuthState::AUTHENTICATED;
                send_line(reply::make(reply::AUTH_SUCCESS, "Authentication successful"));
            } else {
                send_line(reply::make(reply::AUTH_INVALID, "Authentication failed"));
                auth_state_ = AuthState::NONE;
            }
            break;
        }

        case AuthState::LOGIN_WAITING_USERNAME:
            auth_username_ = decoded;
            auth_state_ = AuthState::LOGIN_WAITING_PASSWORD;
            send_line(reply::make(reply::AUTH_CONTINUE, base64_encode("Password:")));
            break;

        case AuthState::LOGIN_WAITING_PASSWORD: {
            std::string password = decoded;

            if (auth_->authenticate(auth_username_, password)) {
                set_authenticated(true);
                auto [user, domain] = Authenticator::parse_email(auth_username_);
                set_username(user);
                set_domain(domain);
                auth_state_ = AuthState::AUTHENTICATED;
                send_line(reply::make(reply::AUTH_SUCCESS, "Authentication successful"));
            } else {
                send_line(reply::make(reply::AUTH_INVALID, "Authentication failed"));
                auth_state_ = AuthState::NONE;
            }
            auth_username_.clear();
            break;
        }

        default:
            auth_state_ = AuthState::NONE;
            break;
    }
}

bool SMTPSession::is_local_domain(const std::string& domain) const {
    for (const auto& local : local_domains_) {
        if (local == domain) {
            return true;
        }
    }
    return auth_->is_local_domain(domain);
}

bool SMTPSession::deliver_message() {
    if (envelope_.rcpt_to.empty()) {
        return false;
    }

    // Add Received header
    std::ostringstream received;
    received << "Received: from " << client_hostname_
             << " (" << remote_address() << ")\r\n"
             << "\tby " << hostname_ << " with "
             << (is_tls() ? "ESMTPS" : "ESMTP") << ";\r\n"
             << "\t" << get_timestamp() << "\r\n";

    std::string full_message = received.str() + data_buffer_;
    envelope_.data = full_message;

    bool all_delivered = true;

    for (const auto& recipient : envelope_.rcpt_to) {
        auto addr = EmailAddress::parse(recipient);
        if (!addr) {
            LOG_ERROR_FMT("Invalid recipient address: {}", recipient);
            all_delivered = false;
            continue;
        }

        if (is_local_domain(addr->domain)) {
            // Local delivery
            if (!relay_->deliver_local(addr->domain, addr->local_part,
                                       full_message, maildir_root_)) {
                all_delivered = false;
            }
        } else if (allow_relay_ || is_authenticated()) {
            // Remote delivery
            auto result = relay_->deliver_remote(recipient, envelope_.mail_from, full_message);
            if (!result.success) {
                LOG_ERROR_FMT("Failed to relay to {}: {}", recipient, result.error);
                all_delivered = false;
            }
        } else {
            LOG_WARNING_FMT("Relay denied for {}", recipient);
            all_delivered = false;
        }
    }

    return all_delivered;
}

}  // namespace email::smtp
