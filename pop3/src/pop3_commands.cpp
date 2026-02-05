#include "pop3_commands.hpp"
#include "pop3_session.hpp"
#include "logger.hpp"
#include <algorithm>
#include <sstream>
#include <cctype>

namespace email::pop3 {

Command Command::parse(const std::string& line) {
    Command cmd;
    cmd.type = CommandType::UNKNOWN;

    if (line.empty()) {
        return cmd;
    }

    std::istringstream iss(line);
    std::string name;
    iss >> name;

    // Convert to uppercase for comparison
    std::transform(name.begin(), name.end(), name.begin(), ::toupper);
    cmd.name = name;
    cmd.type = string_to_type(name);

    // Get the rest as argument
    std::getline(iss >> std::ws, cmd.argument);

    // Also split into individual args
    std::istringstream arg_stream(cmd.argument);
    std::string arg;
    while (arg_stream >> arg) {
        cmd.args.push_back(arg);
    }

    return cmd;
}

CommandType Command::string_to_type(const std::string& name) {
    static const std::unordered_map<std::string, CommandType> mapping = {
        {"USER", CommandType::USER},
        {"PASS", CommandType::PASS},
        {"STAT", CommandType::STAT},
        {"LIST", CommandType::LIST},
        {"RETR", CommandType::RETR},
        {"DELE", CommandType::DELE},
        {"NOOP", CommandType::NOOP},
        {"RSET", CommandType::RSET},
        {"QUIT", CommandType::QUIT},
        {"TOP", CommandType::TOP},
        {"UIDL", CommandType::UIDL},
        {"CAPA", CommandType::CAPA},
        {"STLS", CommandType::STLS},
        {"AUTH", CommandType::AUTH},
        {"APOP", CommandType::APOP}
    };

    auto it = mapping.find(name);
    return it != mapping.end() ? it->second : CommandType::UNKNOWN;
}

std::string Command::type_to_string(CommandType type) {
    switch (type) {
        case CommandType::USER: return "USER";
        case CommandType::PASS: return "PASS";
        case CommandType::STAT: return "STAT";
        case CommandType::LIST: return "LIST";
        case CommandType::RETR: return "RETR";
        case CommandType::DELE: return "DELE";
        case CommandType::NOOP: return "NOOP";
        case CommandType::RSET: return "RSET";
        case CommandType::QUIT: return "QUIT";
        case CommandType::TOP: return "TOP";
        case CommandType::UIDL: return "UIDL";
        case CommandType::CAPA: return "CAPA";
        case CommandType::STLS: return "STLS";
        case CommandType::AUTH: return "AUTH";
        case CommandType::APOP: return "APOP";
        default: return "UNKNOWN";
    }
}

CommandHandler& CommandHandler::instance() {
    static CommandHandler instance;
    return instance;
}

CommandHandler::CommandHandler() {
    handlers_[CommandType::USER] = handle_user;
    handlers_[CommandType::PASS] = handle_pass;
    handlers_[CommandType::STAT] = handle_stat;
    handlers_[CommandType::LIST] = handle_list;
    handlers_[CommandType::RETR] = handle_retr;
    handlers_[CommandType::DELE] = handle_dele;
    handlers_[CommandType::NOOP] = handle_noop;
    handlers_[CommandType::RSET] = handle_rset;
    handlers_[CommandType::QUIT] = handle_quit;
    handlers_[CommandType::TOP] = handle_top;
    handlers_[CommandType::UIDL] = handle_uidl;
    handlers_[CommandType::CAPA] = handle_capa;
    handlers_[CommandType::STLS] = handle_stls;
    handlers_[CommandType::AUTH] = handle_auth;
}

void CommandHandler::register_handler(CommandType type, Handler handler) {
    handlers_[type] = std::move(handler);
}

std::string CommandHandler::execute(POP3Session& session, const Command& cmd) {
    auto it = handlers_.find(cmd.type);
    if (it != handlers_.end()) {
        return it->second(session, cmd);
    }
    return response::err("Unknown command");
}

std::string CommandHandler::handle_user(POP3Session& session, const Command& cmd) {
    if (session.state() != SessionState::AUTHORIZATION) {
        return response::err("Already authenticated");
    }

    if (cmd.argument.empty()) {
        return response::err("Username required");
    }

    session.set_pending_username(cmd.argument);
    return response::ok("User accepted");
}

std::string CommandHandler::handle_pass(POP3Session& session, const Command& cmd) {
    if (session.state() != SessionState::AUTHORIZATION) {
        return response::err("Already authenticated");
    }

    if (session.pending_username().empty()) {
        return response::err("USER command required first");
    }

    if (cmd.argument.empty()) {
        return response::err("Password required");
    }

    if (session.authenticator().authenticate(session.pending_username(), cmd.argument)) {
        session.set_authenticated(true);
        auto [user, domain] = Authenticator::parse_email(session.pending_username());
        session.set_username(user);
        session.set_domain(domain);

        if (!session.open_maildir()) {
            return response::err("Unable to open mailbox");
        }

        session.set_state(SessionState::TRANSACTION);
        return response::ok("Authentication successful, " +
                           std::to_string(session.total_messages()) + " messages");
    }

    session.set_pending_username("");
    return response::err("Authentication failed");
}

std::string CommandHandler::handle_stat(POP3Session& session, const Command& /* cmd */) {
    if (session.state() != SessionState::TRANSACTION) {
        return response::err("Not authenticated");
    }

    return response::ok(std::to_string(session.total_messages()) + " " +
                       std::to_string(session.total_size()));
}

std::string CommandHandler::handle_list(POP3Session& session, const Command& cmd) {
    if (session.state() != SessionState::TRANSACTION) {
        return response::err("Not authenticated");
    }

    if (!cmd.argument.empty()) {
        // Single message listing
        size_t msg_num = std::stoul(cmd.argument);
        auto msg = session.get_message(msg_num);
        if (!msg || msg->deleted) {
            return response::err("No such message");
        }
        return response::ok(std::to_string(msg->number) + " " + std::to_string(msg->size));
    }

    // Multi-line response
    std::ostringstream oss;
    oss << response::ok(std::to_string(session.total_messages()) + " messages ("
                       + std::to_string(session.total_size()) + " octets)") << "\r\n";

    for (const auto& msg : session.messages()) {
        if (!msg.deleted) {
            oss << msg.number << " " << msg.size << "\r\n";
        }
    }
    oss << ".";

    return oss.str();
}

std::string CommandHandler::handle_retr(POP3Session& session, const Command& cmd) {
    if (session.state() != SessionState::TRANSACTION) {
        return response::err("Not authenticated");
    }

    if (cmd.argument.empty()) {
        return response::err("Message number required");
    }

    size_t msg_num = std::stoul(cmd.argument);
    auto msg = session.get_message(msg_num);
    if (!msg || msg->deleted) {
        return response::err("No such message");
    }

    auto content = session.get_message_content(msg_num);
    if (!content) {
        return response::err("Unable to retrieve message");
    }

    std::ostringstream oss;
    oss << response::ok(std::to_string(content->size()) + " octets") << "\r\n";

    // Byte-stuff lines starting with .
    std::istringstream iss(*content);
    std::string line;
    while (std::getline(iss, line)) {
        if (!line.empty() && line[0] == '.') {
            oss << ".";
        }
        // Remove trailing \r if present (we'll add \r\n)
        if (!line.empty() && line.back() == '\r') {
            line.pop_back();
        }
        oss << line << "\r\n";
    }
    oss << ".";

    return oss.str();
}

std::string CommandHandler::handle_dele(POP3Session& session, const Command& cmd) {
    if (session.state() != SessionState::TRANSACTION) {
        return response::err("Not authenticated");
    }

    if (cmd.argument.empty()) {
        return response::err("Message number required");
    }

    size_t msg_num = std::stoul(cmd.argument);
    auto msg = session.get_message(msg_num);
    if (!msg) {
        return response::err("No such message");
    }

    if (msg->deleted) {
        return response::err("Message already deleted");
    }

    if (session.mark_deleted(msg_num)) {
        return response::ok("Message " + std::to_string(msg_num) + " deleted");
    }

    return response::err("Unable to delete message");
}

std::string CommandHandler::handle_noop(POP3Session& session, const Command& /* cmd */) {
    if (session.state() != SessionState::TRANSACTION) {
        return response::err("Not authenticated");
    }
    return response::ok();
}

std::string CommandHandler::handle_rset(POP3Session& session, const Command& /* cmd */) {
    if (session.state() != SessionState::TRANSACTION) {
        return response::err("Not authenticated");
    }

    session.reset_deleted();
    return response::ok(std::to_string(session.total_messages()) + " messages restored");
}

std::string CommandHandler::handle_quit(POP3Session& session, const Command& /* cmd */) {
    if (session.state() == SessionState::TRANSACTION) {
        session.set_state(SessionState::UPDATE);
        size_t deleted = session.expunge();
        return response::ok("Goodbye, " + std::to_string(deleted) + " messages deleted");
    }
    return response::ok("Goodbye");
}

std::string CommandHandler::handle_top(POP3Session& session, const Command& cmd) {
    if (session.state() != SessionState::TRANSACTION) {
        return response::err("Not authenticated");
    }

    if (cmd.args.size() < 2) {
        return response::err("Usage: TOP msg n");
    }

    size_t msg_num = std::stoul(cmd.args[0]);
    size_t lines = std::stoul(cmd.args[1]);

    auto msg = session.get_message(msg_num);
    if (!msg || msg->deleted) {
        return response::err("No such message");
    }

    auto content = session.get_message_top(msg_num, lines);
    if (!content) {
        return response::err("Unable to retrieve message");
    }

    std::ostringstream oss;
    oss << response::ok() << "\r\n";

    // Byte-stuff lines starting with .
    std::istringstream iss(*content);
    std::string line;
    while (std::getline(iss, line)) {
        if (!line.empty() && line[0] == '.') {
            oss << ".";
        }
        if (!line.empty() && line.back() == '\r') {
            line.pop_back();
        }
        oss << line << "\r\n";
    }
    oss << ".";

    return oss.str();
}

std::string CommandHandler::handle_uidl(POP3Session& session, const Command& cmd) {
    if (session.state() != SessionState::TRANSACTION) {
        return response::err("Not authenticated");
    }

    if (!cmd.argument.empty()) {
        // Single message UIDL
        size_t msg_num = std::stoul(cmd.argument);
        auto msg = session.get_message(msg_num);
        if (!msg || msg->deleted) {
            return response::err("No such message");
        }
        return response::ok(std::to_string(msg->number) + " " + msg->unique_id);
    }

    // Multi-line response
    std::ostringstream oss;
    oss << response::ok() << "\r\n";

    for (const auto& msg : session.messages()) {
        if (!msg.deleted) {
            oss << msg.number << " " << msg.unique_id << "\r\n";
        }
    }
    oss << ".";

    return oss.str();
}

std::string CommandHandler::handle_capa(POP3Session& session, const Command& /* cmd */) {
    std::ostringstream oss;
    oss << response::ok("Capability list follows") << "\r\n";
    oss << "USER\r\n";
    oss << "TOP\r\n";
    oss << "UIDL\r\n";
    oss << "RESP-CODES\r\n";
    oss << "AUTH-RESP-CODE\r\n";
    oss << "PIPELINING\r\n";

    if (session.starttls_available()) {
        oss << "STLS\r\n";
    }

    if (session.state() == SessionState::TRANSACTION) {
        oss << "EXPIRE NEVER\r\n";
    }

    oss << "IMPLEMENTATION " << session.hostname() << "\r\n";
    oss << ".";

    return oss.str();
}

std::string CommandHandler::handle_stls(POP3Session& session, const Command& /* cmd */) {
#ifdef ENABLE_TLS
    if (session.is_tls()) {
        return response::err("Already using TLS");
    }

    if (session.state() != SessionState::AUTHORIZATION) {
        return response::err("STLS only allowed before authentication");
    }

    if (!session.starttls_available()) {
        return response::err("STLS not available");
    }

    auto* ssl_ctx = session.ssl_context();
    if (!ssl_ctx) {
        return response::err("TLS not configured");
    }

    // Response sent before TLS handshake
    session.send_line(response::ok("Begin TLS negotiation"));
    session.start_tls(*ssl_ctx);

    return "";  // Response already sent
#else
    (void)session;
    return response::err("TLS not supported");
#endif
}

std::string CommandHandler::handle_auth(POP3Session& session, const Command& cmd) {
    if (session.state() != SessionState::AUTHORIZATION) {
        return response::err("Already authenticated");
    }

    if (cmd.argument.empty()) {
        // List supported mechanisms
        std::ostringstream oss;
        oss << response::ok() << "\r\n";
        oss << "PLAIN\r\n";
        oss << "LOGIN\r\n";
        oss << ".";
        return oss.str();
    }

    std::string mechanism = cmd.argument;
    std::transform(mechanism.begin(), mechanism.end(), mechanism.begin(), ::toupper);

    if (mechanism == "PLAIN" || mechanism == "LOGIN") {
        // These would require multi-step authentication
        // For simplicity, we only support USER/PASS
        return response::err("Use USER/PASS for authentication");
    }

    return response::err("Unknown authentication mechanism");
}

}  // namespace email::pop3
