#include "smtp_commands.hpp"
#include "smtp_session.hpp"
#include "logger.hpp"
#include <algorithm>
#include <sstream>
#include <cctype>
#include <regex>

// Base64 decode helper
#include <openssl/bio.h>
#include <openssl/evp.h>
#include <openssl/buffer.h>

namespace email::smtp {

namespace {

std::string base64_decode(const std::string& encoded) {
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

}  // namespace

Command Command::parse(const std::string& line) {
    Command cmd;
    cmd.type = CommandType::UNKNOWN;

    if (line.empty()) {
        return cmd;
    }

    // Find the first space or colon
    size_t sep = line.find_first_of(" :");
    std::string name;

    if (sep == std::string::npos) {
        name = line;
    } else {
        name = line.substr(0, sep);
        // Handle commands like "MAIL FROM:" and "RCPT TO:"
        if (line[sep] == ' ') {
            size_t next_space = line.find(' ', sep + 1);
            if (next_space != std::string::npos) {
                std::string potential_qualifier = line.substr(sep + 1, next_space - sep - 1);
                std::transform(potential_qualifier.begin(), potential_qualifier.end(),
                              potential_qualifier.begin(), ::toupper);
                if (potential_qualifier == "FROM:" || potential_qualifier == "TO:") {
                    name += " " + potential_qualifier;
                    sep = next_space;
                }
            } else {
                std::string rest = line.substr(sep + 1);
                std::transform(rest.begin(), rest.end(), rest.begin(), ::toupper);
                if (rest == "FROM:" || rest == "TO:") {
                    name += " " + rest;
                    sep = line.length();
                }
            }
        }
    }

    std::transform(name.begin(), name.end(), name.begin(), ::toupper);
    cmd.name = name;
    cmd.type = string_to_type(name);

    // Get argument
    if (sep < line.length()) {
        cmd.argument = line.substr(sep);
        // Trim leading whitespace and colon
        auto start = cmd.argument.find_first_not_of(" :");
        if (start != std::string::npos) {
            cmd.argument = cmd.argument.substr(start);
        } else {
            cmd.argument.clear();
        }
    }

    return cmd;
}

CommandType Command::string_to_type(const std::string& name) {
    static const std::unordered_map<std::string, CommandType> mapping = {
        {"HELO", CommandType::HELO},
        {"EHLO", CommandType::EHLO},
        {"MAIL FROM:", CommandType::MAIL},
        {"MAIL", CommandType::MAIL},
        {"RCPT TO:", CommandType::RCPT},
        {"RCPT", CommandType::RCPT},
        {"DATA", CommandType::DATA},
        {"RSET", CommandType::RSET},
        {"NOOP", CommandType::NOOP},
        {"QUIT", CommandType::QUIT},
        {"VRFY", CommandType::VRFY},
        {"AUTH", CommandType::AUTH},
        {"STARTTLS", CommandType::STARTTLS},
        {"HELP", CommandType::HELP}
    };

    auto it = mapping.find(name);
    return it != mapping.end() ? it->second : CommandType::UNKNOWN;
}

std::string Command::type_to_string(CommandType type) {
    switch (type) {
        case CommandType::HELO: return "HELO";
        case CommandType::EHLO: return "EHLO";
        case CommandType::MAIL: return "MAIL";
        case CommandType::RCPT: return "RCPT";
        case CommandType::DATA: return "DATA";
        case CommandType::RSET: return "RSET";
        case CommandType::NOOP: return "NOOP";
        case CommandType::QUIT: return "QUIT";
        case CommandType::VRFY: return "VRFY";
        case CommandType::AUTH: return "AUTH";
        case CommandType::STARTTLS: return "STARTTLS";
        case CommandType::HELP: return "HELP";
        default: return "UNKNOWN";
    }
}

std::optional<EmailAddress> EmailAddress::parse(const std::string& str) {
    // Extract email from angle brackets if present
    std::string email = str;
    auto start = email.find('<');
    auto end = email.find('>');

    if (start != std::string::npos && end != std::string::npos && end > start) {
        email = email.substr(start + 1, end - start - 1);
    }

    // Trim whitespace
    auto trim_start = email.find_first_not_of(" \t");
    auto trim_end = email.find_last_not_of(" \t");
    if (trim_start != std::string::npos && trim_end != std::string::npos) {
        email = email.substr(trim_start, trim_end - trim_start + 1);
    }

    // Find @
    auto at = email.find('@');
    if (at == std::string::npos || at == 0 || at == email.length() - 1) {
        // Allow null sender <>
        if (email.empty()) {
            return EmailAddress{"", "", ""};
        }
        return std::nullopt;
    }

    EmailAddress addr;
    addr.local_part = email.substr(0, at);
    addr.domain = email.substr(at + 1);
    addr.full_address = email;

    return addr;
}

CommandHandler& CommandHandler::instance() {
    static CommandHandler instance;
    return instance;
}

CommandHandler::CommandHandler() {
    handlers_[CommandType::HELO] = handle_helo;
    handlers_[CommandType::EHLO] = handle_ehlo;
    handlers_[CommandType::MAIL] = handle_mail;
    handlers_[CommandType::RCPT] = handle_rcpt;
    handlers_[CommandType::DATA] = handle_data;
    handlers_[CommandType::RSET] = handle_rset;
    handlers_[CommandType::NOOP] = handle_noop;
    handlers_[CommandType::QUIT] = handle_quit;
    handlers_[CommandType::VRFY] = handle_vrfy;
    handlers_[CommandType::AUTH] = handle_auth;
    handlers_[CommandType::STARTTLS] = handle_starttls;
    handlers_[CommandType::HELP] = handle_help;
}

void CommandHandler::register_handler(CommandType type, Handler handler) {
    handlers_[type] = std::move(handler);
}

std::string CommandHandler::execute(SMTPSession& session, const Command& cmd) {
    auto it = handlers_.find(cmd.type);
    if (it != handlers_.end()) {
        return it->second(session, cmd);
    }
    return reply::make(reply::COMMAND_NOT_IMPLEMENTED, "Command not implemented");
}

std::string CommandHandler::handle_helo(SMTPSession& session, const Command& cmd) {
    if (cmd.argument.empty()) {
        return reply::make(reply::SYNTAX_ERROR_PARAMS, "Hostname required");
    }

    session.set_client_hostname(cmd.argument);
    session.set_state(SessionState::GREETED);
    session.envelope().clear();

    return reply::make(reply::OK, session.hostname() + " Hello " + cmd.argument);
}

std::string CommandHandler::handle_ehlo(SMTPSession& session, const Command& cmd) {
    if (cmd.argument.empty()) {
        return reply::make(reply::SYNTAX_ERROR_PARAMS, "Hostname required");
    }

    session.set_client_hostname(cmd.argument);
    session.set_state(SessionState::GREETED);
    session.envelope().clear();

    std::vector<std::string> capabilities;
    capabilities.push_back(session.hostname() + " Hello " + cmd.argument);
    capabilities.push_back("SIZE " + std::to_string(session.max_message_size()));
    capabilities.push_back("8BITMIME");
    capabilities.push_back("PIPELINING");

    if (session.starttls_available()) {
        capabilities.push_back("STARTTLS");
    }

    if (!session.is_authenticated()) {
        capabilities.push_back("AUTH PLAIN LOGIN");
    }

    return reply::make_multi(reply::OK, capabilities);
}

std::string CommandHandler::handle_mail(SMTPSession& session, const Command& cmd) {
    if (session.state() != SessionState::GREETED &&
        session.state() != SessionState::MAIL &&
        session.state() != SessionState::RCPT) {
        return reply::make(reply::BAD_SEQUENCE, "Send HELO/EHLO first");
    }

    // Check authentication requirement
    if (session.require_auth() && !session.is_authenticated()) {
        return reply::make(reply::AUTH_REQUIRED, "Authentication required");
    }

    // Parse FROM address
    auto addr = EmailAddress::parse(cmd.argument);
    if (!addr) {
        return reply::make(reply::SYNTAX_ERROR_PARAMS, "Invalid sender address");
    }

    session.envelope().clear();
    session.envelope().mail_from = addr->full_address;
    session.set_state(SessionState::MAIL);

    return reply::make(reply::OK, "OK");
}

std::string CommandHandler::handle_rcpt(SMTPSession& session, const Command& cmd) {
    if (session.state() != SessionState::MAIL && session.state() != SessionState::RCPT) {
        return reply::make(reply::BAD_SEQUENCE, "Send MAIL FROM first");
    }

    if (session.envelope().rcpt_to.size() >= session.max_recipients()) {
        return reply::make(reply::EXCEEDED_STORAGE, "Too many recipients");
    }

    // Parse TO address
    auto addr = EmailAddress::parse(cmd.argument);
    if (!addr || addr->domain.empty()) {
        return reply::make(reply::SYNTAX_ERROR_PARAMS, "Invalid recipient address");
    }

    // Check if we can accept mail for this recipient
    bool is_local = session.is_local_domain(addr->domain);

    if (!is_local && !session.allow_relay()) {
        if (!session.is_authenticated()) {
            return reply::make(reply::AUTH_REQUIRED, "Relay access denied");
        }
    }

    // Verify local user exists
    if (is_local) {
        auto user = session.authenticator().get_user(addr->full_address);
        if (!user) {
            return reply::make(reply::MAILBOX_NOT_FOUND, "User not found");
        }
    }

    session.envelope().rcpt_to.push_back(addr->full_address);
    session.set_state(SessionState::RCPT);

    return reply::make(reply::OK, "OK");
}

std::string CommandHandler::handle_data(SMTPSession& session, const Command& /* cmd */) {
    if (session.state() != SessionState::RCPT) {
        return reply::make(reply::BAD_SEQUENCE, "Send RCPT TO first");
    }

    if (session.envelope().rcpt_to.empty()) {
        return reply::make(reply::BAD_SEQUENCE, "No recipients");
    }

    session.set_state(SessionState::DATA);
    return reply::make(reply::START_MAIL_INPUT, "Start mail input; end with <CRLF>.<CRLF>");
}

std::string CommandHandler::handle_rset(SMTPSession& session, const Command& /* cmd */) {
    session.envelope().clear();
    if (session.state() != SessionState::CONNECTED) {
        session.set_state(SessionState::GREETED);
    }

    return reply::make(reply::OK, "OK");
}

std::string CommandHandler::handle_noop(SMTPSession& /* session */, const Command& /* cmd */) {
    return reply::make(reply::OK, "OK");
}

std::string CommandHandler::handle_quit(SMTPSession& session, const Command& /* cmd */) {
    session.set_state(SessionState::QUIT);
    return reply::make(reply::SERVICE_CLOSING, session.hostname() + " closing connection");
}

std::string CommandHandler::handle_vrfy(SMTPSession& session, const Command& cmd) {
    if (cmd.argument.empty()) {
        return reply::make(reply::SYNTAX_ERROR_PARAMS, "Address required");
    }

    // Parse address
    auto addr = EmailAddress::parse(cmd.argument);
    if (!addr) {
        return reply::make(reply::SYNTAX_ERROR_PARAMS, "Invalid address");
    }

    // Check if user exists
    if (session.is_local_domain(addr->domain)) {
        auto user = session.authenticator().get_user(addr->full_address);
        if (user) {
            return reply::make(reply::OK, addr->full_address);
        }
    }

    return reply::make(reply::CANNOT_VRFY, "Cannot verify user");
}

std::string CommandHandler::handle_auth(SMTPSession& session, const Command& cmd) {
    if (session.state() == SessionState::CONNECTED) {
        return reply::make(reply::BAD_SEQUENCE, "Send HELO/EHLO first");
    }

    if (session.is_authenticated()) {
        return reply::make(reply::BAD_SEQUENCE, "Already authenticated");
    }

    std::istringstream iss(cmd.argument);
    std::string mechanism;
    std::string initial_response;
    iss >> mechanism >> initial_response;

    std::transform(mechanism.begin(), mechanism.end(), mechanism.begin(), ::toupper);

    if (mechanism == "PLAIN") {
        if (initial_response.empty()) {
            // Wait for client to send credentials
            session.set_auth_state(AuthState::PLAIN_WAITING);
            return reply::make(reply::AUTH_CONTINUE, "");
        }

        // Decode and parse PLAIN credentials
        std::string decoded = base64_decode(initial_response);
        // PLAIN format: \0username\0password
        size_t first_null = decoded.find('\0');
        size_t second_null = decoded.find('\0', first_null + 1);

        if (first_null == std::string::npos || second_null == std::string::npos) {
            return reply::make(reply::AUTH_INVALID, "Invalid credentials");
        }

        std::string username = decoded.substr(first_null + 1, second_null - first_null - 1);
        std::string password = decoded.substr(second_null + 1);

        if (session.authenticator().authenticate(username, password)) {
            session.set_authenticated(true);
            auto [user, domain] = Authenticator::parse_email(username);
            session.set_username(user);
            session.set_domain(domain);
            session.set_auth_state(AuthState::AUTHENTICATED);
            return reply::make(reply::AUTH_SUCCESS, "Authentication successful");
        }

        return reply::make(reply::AUTH_INVALID, "Authentication failed");
    } else if (mechanism == "LOGIN") {
        session.set_auth_state(AuthState::LOGIN_WAITING_USERNAME);
        return reply::make(reply::AUTH_CONTINUE, base64_encode("Username:"));
    }

    return reply::make(reply::PARAM_NOT_IMPLEMENTED, "Unknown authentication mechanism");
}

std::string CommandHandler::handle_starttls(SMTPSession& session, const Command& /* cmd */) {
#ifdef ENABLE_TLS
    if (session.is_tls()) {
        return reply::make(reply::BAD_SEQUENCE, "Already using TLS");
    }

    if (!session.starttls_available()) {
        return reply::make(reply::COMMAND_NOT_IMPLEMENTED, "STARTTLS not available");
    }

    auto* ssl_ctx = session.ssl_context();
    if (!ssl_ctx) {
        return reply::make(reply::LOCAL_ERROR, "TLS not configured");
    }

    // Send response before TLS handshake
    session.send_line(reply::make(reply::SERVICE_READY, "Ready to start TLS"));
    session.start_tls(*ssl_ctx);

    // Reset session state after TLS
    session.set_state(SessionState::CONNECTED);
    session.envelope().clear();

    return "";  // Response already sent
#else
    (void)session;
    return reply::make(reply::COMMAND_NOT_IMPLEMENTED, "TLS not supported");
#endif
}

std::string CommandHandler::handle_help(SMTPSession& session, const Command& /* cmd */) {
    std::vector<std::string> help;
    help.push_back(session.hostname() + " supports:");
    help.push_back("HELO EHLO MAIL RCPT DATA RSET NOOP QUIT VRFY AUTH STARTTLS HELP");

    return reply::make_multi(reply::HELP, help);
}

}  // namespace email::smtp
