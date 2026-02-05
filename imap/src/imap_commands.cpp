#include "imap_commands.hpp"
#include "imap_session.hpp"
#include "imap_parser.hpp"
#include "logger.hpp"
#include <algorithm>
#include <sstream>
#include <cctype>

namespace email::imap {

Command Command::parse(const std::string& line) {
    Command cmd;
    cmd.type = CommandType::UNKNOWN;

    std::string tag, command, arguments;
    if (!IMAPParser::parse_command(line, tag, command, arguments)) {
        return cmd;
    }

    cmd.tag = tag;
    cmd.name = command;
    cmd.arguments = arguments;
    cmd.type = string_to_type(command);

    return cmd;
}

CommandType Command::string_to_type(const std::string& name) {
    static const std::unordered_map<std::string, CommandType> mapping = {
        {"CAPABILITY", CommandType::CAPABILITY},
        {"NOOP", CommandType::NOOP},
        {"LOGOUT", CommandType::LOGOUT},
        {"STARTTLS", CommandType::STARTTLS},
        {"AUTHENTICATE", CommandType::AUTHENTICATE},
        {"LOGIN", CommandType::LOGIN},
        {"SELECT", CommandType::SELECT},
        {"EXAMINE", CommandType::EXAMINE},
        {"CREATE", CommandType::CREATE},
        {"DELETE", CommandType::DELETE},
        {"RENAME", CommandType::RENAME},
        {"SUBSCRIBE", CommandType::SUBSCRIBE},
        {"UNSUBSCRIBE", CommandType::UNSUBSCRIBE},
        {"LIST", CommandType::LIST},
        {"LSUB", CommandType::LSUB},
        {"STATUS", CommandType::STATUS},
        {"APPEND", CommandType::APPEND},
        {"CHECK", CommandType::CHECK},
        {"CLOSE", CommandType::CLOSE},
        {"EXPUNGE", CommandType::EXPUNGE},
        {"SEARCH", CommandType::SEARCH},
        {"FETCH", CommandType::FETCH},
        {"STORE", CommandType::STORE},
        {"COPY", CommandType::COPY},
        {"UID", CommandType::UID}
    };

    auto it = mapping.find(name);
    return it != mapping.end() ? it->second : CommandType::UNKNOWN;
}

std::string Command::type_to_string(CommandType type) {
    switch (type) {
        case CommandType::CAPABILITY: return "CAPABILITY";
        case CommandType::NOOP: return "NOOP";
        case CommandType::LOGOUT: return "LOGOUT";
        case CommandType::STARTTLS: return "STARTTLS";
        case CommandType::AUTHENTICATE: return "AUTHENTICATE";
        case CommandType::LOGIN: return "LOGIN";
        case CommandType::SELECT: return "SELECT";
        case CommandType::EXAMINE: return "EXAMINE";
        case CommandType::CREATE: return "CREATE";
        case CommandType::DELETE: return "DELETE";
        case CommandType::RENAME: return "RENAME";
        case CommandType::SUBSCRIBE: return "SUBSCRIBE";
        case CommandType::UNSUBSCRIBE: return "UNSUBSCRIBE";
        case CommandType::LIST: return "LIST";
        case CommandType::LSUB: return "LSUB";
        case CommandType::STATUS: return "STATUS";
        case CommandType::APPEND: return "APPEND";
        case CommandType::CHECK: return "CHECK";
        case CommandType::CLOSE: return "CLOSE";
        case CommandType::EXPUNGE: return "EXPUNGE";
        case CommandType::SEARCH: return "SEARCH";
        case CommandType::FETCH: return "FETCH";
        case CommandType::STORE: return "STORE";
        case CommandType::COPY: return "COPY";
        case CommandType::UID: return "UID";
        default: return "UNKNOWN";
    }
}

CommandHandler& CommandHandler::instance() {
    static CommandHandler instance;
    return instance;
}

CommandHandler::CommandHandler() {
    handlers_[CommandType::CAPABILITY] = handle_capability;
    handlers_[CommandType::NOOP] = handle_noop;
    handlers_[CommandType::LOGOUT] = handle_logout;
    handlers_[CommandType::STARTTLS] = handle_starttls;
    handlers_[CommandType::LOGIN] = handle_login;
    handlers_[CommandType::SELECT] = handle_select;
    handlers_[CommandType::EXAMINE] = handle_examine;
    handlers_[CommandType::CREATE] = handle_create;
    handlers_[CommandType::DELETE] = handle_delete;
    handlers_[CommandType::RENAME] = handle_rename;
    handlers_[CommandType::LIST] = handle_list;
    handlers_[CommandType::LSUB] = handle_lsub;
    handlers_[CommandType::STATUS] = handle_status;
    handlers_[CommandType::APPEND] = handle_append;
    handlers_[CommandType::CHECK] = handle_check;
    handlers_[CommandType::CLOSE] = handle_close;
    handlers_[CommandType::EXPUNGE] = handle_expunge;
    handlers_[CommandType::SEARCH] = handle_search;
    handlers_[CommandType::FETCH] = handle_fetch;
    handlers_[CommandType::STORE] = handle_store;
    handlers_[CommandType::COPY] = handle_copy;
    handlers_[CommandType::UID] = handle_uid;
}

void CommandHandler::register_handler(CommandType type, Handler handler) {
    handlers_[type] = std::move(handler);
}

std::vector<std::string> CommandHandler::execute(IMAPSession& session, const Command& cmd) {
    auto it = handlers_.find(cmd.type);
    if (it != handlers_.end()) {
        return it->second(session, cmd);
    }
    return {response::bad(cmd.tag, "Unknown command")};
}

std::vector<std::string> CommandHandler::handle_capability(IMAPSession& session, const Command& cmd) {
    std::vector<std::string> responses;

    std::string caps = "CAPABILITY IMAP4rev1 AUTH=PLAIN AUTH=LOGIN";

    if (session.starttls_available()) {
        caps += " STARTTLS";
    }

    if (session.state() == SessionState::AUTHENTICATED ||
        session.state() == SessionState::SELECTED) {
        caps += " CHILDREN NAMESPACE";
    }

    responses.push_back(response::untagged(caps));
    responses.push_back(response::ok(cmd.tag, "CAPABILITY completed"));

    return responses;
}

std::vector<std::string> CommandHandler::handle_noop(IMAPSession& /* session */, const Command& cmd) {
    return {response::ok(cmd.tag, "NOOP completed")};
}

std::vector<std::string> CommandHandler::handle_logout(IMAPSession& session, const Command& cmd) {
    session.set_state(SessionState::LOGOUT);
    return {
        response::bye("Logging out"),
        response::ok(cmd.tag, "LOGOUT completed")
    };
}

std::vector<std::string> CommandHandler::handle_starttls(IMAPSession& session, const Command& cmd) {
#ifdef ENABLE_TLS
    if (session.is_tls()) {
        return {response::bad(cmd.tag, "Already using TLS")};
    }

    if (session.state() != SessionState::NOT_AUTHENTICATED) {
        return {response::bad(cmd.tag, "STARTTLS only allowed before authentication")};
    }

    auto* ssl_ctx = session.ssl_context();
    if (!ssl_ctx) {
        return {response::no(cmd.tag, "TLS not configured")};
    }

    // Send response before TLS handshake
    session.send_line(response::ok(cmd.tag, "Begin TLS negotiation"));
    session.start_tls(*ssl_ctx);

    return {};  // Response already sent
#else
    (void)session;
    return {response::no(cmd.tag, "TLS not supported")};
#endif
}

std::vector<std::string> CommandHandler::handle_login(IMAPSession& session, const Command& cmd) {
    if (session.state() != SessionState::NOT_AUTHENTICATED) {
        return {response::bad(cmd.tag, "Already authenticated")};
    }

    // Parse username and password
    std::istringstream iss(cmd.arguments);
    std::string username, password;

    size_t pos = 0;
    auto user_opt = IMAPParser::parse_string(cmd.arguments, pos);
    auto pass_opt = IMAPParser::parse_string(cmd.arguments, pos);

    if (!user_opt || !pass_opt) {
        // Try simple space-separated
        iss >> username >> password;
    } else {
        username = *user_opt;
        password = *pass_opt;
    }

    if (username.empty() || password.empty()) {
        return {response::bad(cmd.tag, "Missing username or password")};
    }

    if (session.authenticator().authenticate(username, password)) {
        session.set_authenticated(true);
        auto [user, domain] = Authenticator::parse_email(username);
        session.set_username(user);
        session.set_domain(domain);

        if (!session.open_maildir()) {
            return {response::no(cmd.tag, "Unable to open mailbox")};
        }

        session.set_state(SessionState::AUTHENTICATED);
        return {response::ok(cmd.tag, "LOGIN completed")};
    }

    return {response::no(cmd.tag, "[AUTHENTICATIONFAILED] Authentication failed")};
}

std::vector<std::string> CommandHandler::handle_select(IMAPSession& session, const Command& cmd) {
    if (session.state() != SessionState::AUTHENTICATED &&
        session.state() != SessionState::SELECTED) {
        return {response::bad(cmd.tag, "Not authenticated")};
    }

    std::string mailbox = cmd.arguments;
    // Remove quotes if present
    if (!mailbox.empty() && mailbox.front() == '"') {
        size_t pos = 0;
        auto parsed = IMAPParser::parse_string(cmd.arguments, pos);
        if (parsed) mailbox = *parsed;
    }

    if (mailbox.empty()) {
        return {response::bad(cmd.tag, "Mailbox name required")};
    }

    if (!session.select_mailbox(mailbox, false)) {
        return {response::no(cmd.tag, "Mailbox does not exist")};
    }

    auto* selected = session.selected_mailbox();
    if (!selected) {
        return {response::no(cmd.tag, "Failed to select mailbox")};
    }

    std::vector<std::string> responses;
    responses.push_back(response::untagged(std::to_string(selected->exists) + " EXISTS"));
    responses.push_back(response::untagged(std::to_string(selected->recent) + " RECENT"));

    if (selected->unseen > 0) {
        responses.push_back(response::untagged("OK [UNSEEN " + std::to_string(selected->unseen) + "]"));
    }

    responses.push_back(response::untagged("OK [UIDVALIDITY " + std::to_string(selected->uid_validity) + "]"));
    responses.push_back(response::untagged("OK [UIDNEXT " + std::to_string(selected->uid_next) + "]"));
    responses.push_back(response::untagged("FLAGS (\\Answered \\Flagged \\Deleted \\Seen \\Draft)"));
    responses.push_back(response::untagged("OK [PERMANENTFLAGS (\\Answered \\Flagged \\Deleted \\Seen \\Draft \\*)]"));

    responses.push_back(response::ok(cmd.tag, "[READ-WRITE] SELECT completed"));

    return responses;
}

std::vector<std::string> CommandHandler::handle_examine(IMAPSession& session, const Command& cmd) {
    if (session.state() != SessionState::AUTHENTICATED &&
        session.state() != SessionState::SELECTED) {
        return {response::bad(cmd.tag, "Not authenticated")};
    }

    std::string mailbox = cmd.arguments;
    if (!mailbox.empty() && mailbox.front() == '"') {
        size_t pos = 0;
        auto parsed = IMAPParser::parse_string(cmd.arguments, pos);
        if (parsed) mailbox = *parsed;
    }

    if (!session.select_mailbox(mailbox, true)) {
        return {response::no(cmd.tag, "Mailbox does not exist")};
    }

    auto* selected = session.selected_mailbox();
    std::vector<std::string> responses;
    responses.push_back(response::untagged(std::to_string(selected->exists) + " EXISTS"));
    responses.push_back(response::untagged(std::to_string(selected->recent) + " RECENT"));
    responses.push_back(response::untagged("FLAGS (\\Answered \\Flagged \\Deleted \\Seen \\Draft)"));

    responses.push_back(response::ok(cmd.tag, "[READ-ONLY] EXAMINE completed"));

    return responses;
}

std::vector<std::string> CommandHandler::handle_create(IMAPSession& session, const Command& cmd) {
    if (session.state() != SessionState::AUTHENTICATED &&
        session.state() != SessionState::SELECTED) {
        return {response::bad(cmd.tag, "Not authenticated")};
    }

    std::string mailbox = cmd.arguments;
    if (!mailbox.empty() && mailbox.front() == '"') {
        size_t pos = 0;
        auto parsed = IMAPParser::parse_string(cmd.arguments, pos);
        if (parsed) mailbox = *parsed;
    }

    if (session.maildir()->create_mailbox(mailbox)) {
        return {response::ok(cmd.tag, "CREATE completed")};
    }

    return {response::no(cmd.tag, "Failed to create mailbox")};
}

std::vector<std::string> CommandHandler::handle_delete(IMAPSession& session, const Command& cmd) {
    if (session.state() != SessionState::AUTHENTICATED &&
        session.state() != SessionState::SELECTED) {
        return {response::bad(cmd.tag, "Not authenticated")};
    }

    std::string mailbox = cmd.arguments;
    if (!mailbox.empty() && mailbox.front() == '"') {
        size_t pos = 0;
        auto parsed = IMAPParser::parse_string(cmd.arguments, pos);
        if (parsed) mailbox = *parsed;
    }

    if (session.maildir()->delete_mailbox(mailbox)) {
        return {response::ok(cmd.tag, "DELETE completed")};
    }

    return {response::no(cmd.tag, "Failed to delete mailbox")};
}

std::vector<std::string> CommandHandler::handle_rename(IMAPSession& session, const Command& cmd) {
    if (session.state() != SessionState::AUTHENTICATED &&
        session.state() != SessionState::SELECTED) {
        return {response::bad(cmd.tag, "Not authenticated")};
    }

    size_t pos = 0;
    auto old_name = IMAPParser::parse_string(cmd.arguments, pos);
    auto new_name = IMAPParser::parse_string(cmd.arguments, pos);

    if (!old_name || !new_name) {
        return {response::bad(cmd.tag, "Usage: RENAME old-name new-name")};
    }

    if (session.maildir()->rename_mailbox(*old_name, *new_name)) {
        return {response::ok(cmd.tag, "RENAME completed")};
    }

    return {response::no(cmd.tag, "Failed to rename mailbox")};
}

std::vector<std::string> CommandHandler::handle_list(IMAPSession& session, const Command& cmd) {
    if (session.state() != SessionState::AUTHENTICATED &&
        session.state() != SessionState::SELECTED) {
        return {response::bad(cmd.tag, "Not authenticated")};
    }

    size_t pos = 0;
    auto reference = IMAPParser::parse_string(cmd.arguments, pos);
    auto pattern = IMAPParser::parse_string(cmd.arguments, pos);

    std::string ref = reference.value_or("");
    std::string pat = pattern.value_or("*");

    auto mailboxes = session.list_mailboxes(ref, pat);

    std::vector<std::string> responses;
    for (const auto& mbox : mailboxes) {
        std::string flags = "()";
        if (mbox == "INBOX") {
            flags = "(\\HasNoChildren)";
        }
        responses.push_back(response::untagged("LIST " + flags + " \"/\" " + IMAPParser::quote_string(mbox)));
    }

    responses.push_back(response::ok(cmd.tag, "LIST completed"));
    return responses;
}

std::vector<std::string> CommandHandler::handle_lsub(IMAPSession& session, const Command& cmd) {
    // LSUB is like LIST but for subscribed mailboxes
    // For simplicity, return same as LIST
    return handle_list(session, cmd);
}

std::vector<std::string> CommandHandler::handle_status(IMAPSession& session, const Command& cmd) {
    if (session.state() != SessionState::AUTHENTICATED &&
        session.state() != SessionState::SELECTED) {
        return {response::bad(cmd.tag, "Not authenticated")};
    }

    size_t pos = 0;
    auto mailbox = IMAPParser::parse_string(cmd.arguments, pos);
    if (!mailbox) {
        return {response::bad(cmd.tag, "Mailbox name required")};
    }

    auto info = session.maildir()->get_mailbox_info(*mailbox);
    if (!info) {
        return {response::no(cmd.tag, "Mailbox does not exist")};
    }

    std::ostringstream oss;
    oss << "STATUS " << IMAPParser::quote_string(*mailbox) << " (";
    oss << "MESSAGES " << info->total_messages << " ";
    oss << "RECENT " << info->recent_messages << " ";
    oss << "UNSEEN " << info->unseen_messages << " ";
    oss << "UIDVALIDITY " << info->uid_validity << " ";
    oss << "UIDNEXT " << info->uid_next;
    oss << ")";

    return {
        response::untagged(oss.str()),
        response::ok(cmd.tag, "STATUS completed")
    };
}

std::vector<std::string> CommandHandler::handle_append(IMAPSession& session, const Command& cmd) {
    if (session.state() != SessionState::AUTHENTICATED &&
        session.state() != SessionState::SELECTED) {
        return {response::bad(cmd.tag, "Not authenticated")};
    }

    // APPEND requires literal handling which is complex
    // For now, return a basic response
    return {response::no(cmd.tag, "APPEND not yet implemented")};
}

std::vector<std::string> CommandHandler::handle_check(IMAPSession& session, const Command& cmd) {
    if (session.state() != SessionState::SELECTED) {
        return {response::bad(cmd.tag, "No mailbox selected")};
    }

    return {response::ok(cmd.tag, "CHECK completed")};
}

std::vector<std::string> CommandHandler::handle_close(IMAPSession& session, const Command& cmd) {
    if (session.state() != SessionState::SELECTED) {
        return {response::bad(cmd.tag, "No mailbox selected")};
    }

    session.expunge();
    session.close_mailbox();

    return {response::ok(cmd.tag, "CLOSE completed")};
}

std::vector<std::string> CommandHandler::handle_expunge(IMAPSession& session, const Command& cmd) {
    if (session.state() != SessionState::SELECTED) {
        return {response::bad(cmd.tag, "No mailbox selected")};
    }

    auto deleted = session.expunge();

    std::vector<std::string> responses;
    for (uint32_t seq : deleted) {
        responses.push_back(response::untagged(std::to_string(seq) + " EXPUNGE"));
    }

    responses.push_back(response::ok(cmd.tag, "EXPUNGE completed"));
    return responses;
}

std::vector<std::string> CommandHandler::handle_search(IMAPSession& session, const Command& cmd) {
    if (session.state() != SessionState::SELECTED) {
        return {response::bad(cmd.tag, "No mailbox selected")};
    }

    auto criteria = IMAPParser::parse_search_criteria(cmd.arguments);
    auto results = session.search(criteria, false);

    std::ostringstream oss;
    oss << "SEARCH";
    for (uint32_t seq : results) {
        oss << " " << seq;
    }

    return {
        response::untagged(oss.str()),
        response::ok(cmd.tag, "SEARCH completed")
    };
}

std::vector<std::string> CommandHandler::handle_fetch(IMAPSession& session, const Command& cmd) {
    if (session.state() != SessionState::SELECTED) {
        return {response::bad(cmd.tag, "No mailbox selected")};
    }

    // Parse sequence set and fetch items
    std::istringstream iss(cmd.arguments);
    std::string seq_str, items_str;
    iss >> seq_str;
    std::getline(iss >> std::ws, items_str);

    auto seq_set = IMAPParser::parse_sequence_set(seq_str);
    if (!seq_set) {
        return {response::bad(cmd.tag, "Invalid sequence set")};
    }

    auto items = IMAPParser::parse_fetch_items(items_str);

    std::vector<std::string> responses;

    for (const auto& msg : session.messages()) {
        if (!seq_set->contains(msg.sequence_number)) {
            continue;
        }

        std::ostringstream oss;
        oss << msg.sequence_number << " FETCH (";

        bool first = true;
        for (const auto& item : items) {
            if (!first) oss << " ";
            first = false;

            switch (item.type) {
                case FetchItem::Type::FLAGS:
                    oss << "FLAGS " << IMAPParser::format_flags(msg.flags);
                    break;
                case FetchItem::Type::UID:
                    oss << "UID " << msg.uid;
                    break;
                case FetchItem::Type::RFC822_SIZE:
                    oss << "RFC822.SIZE " << msg.size;
                    break;
                case FetchItem::Type::INTERNALDATE:
                    oss << "INTERNALDATE " << IMAPParser::format_internal_date(msg.internal_date);
                    break;
                case FetchItem::Type::RFC822:
                case FetchItem::Type::BODY:
                case FetchItem::Type::BODY_PEEK: {
                    auto content = session.get_message_content(msg.sequence_number);
                    if (content) {
                        oss << "BODY[] {" << content->size() << "}\r\n" << *content;
                    }
                    break;
                }
                case FetchItem::Type::RFC822_HEADER: {
                    auto headers = session.get_message_headers(msg.sequence_number);
                    if (headers) {
                        oss << "RFC822.HEADER {" << headers->size() << "}\r\n" << *headers;
                    }
                    break;
                }
                default:
                    break;
            }
        }

        oss << ")";
        responses.push_back(response::untagged(oss.str()));
    }

    responses.push_back(response::ok(cmd.tag, "FETCH completed"));
    return responses;
}

std::vector<std::string> CommandHandler::handle_store(IMAPSession& session, const Command& cmd) {
    if (session.state() != SessionState::SELECTED) {
        return {response::bad(cmd.tag, "No mailbox selected")};
    }

    std::istringstream iss(cmd.arguments);
    std::string seq_str, action_str;
    iss >> seq_str;
    std::getline(iss >> std::ws, action_str);

    auto seq_set = IMAPParser::parse_sequence_set(seq_str);
    if (!seq_set) {
        return {response::bad(cmd.tag, "Invalid sequence set")};
    }

    auto action = IMAPParser::parse_store_action(action_str);
    if (!action) {
        return {response::bad(cmd.tag, "Invalid STORE action")};
    }

    std::vector<std::string> responses;
    bool silent = (action->type == StoreAction::Type::FLAGS_SILENT ||
                   action->type == StoreAction::Type::PLUS_FLAGS_SILENT ||
                   action->type == StoreAction::Type::MINUS_FLAGS_SILENT);

    for (const auto& msg : session.messages()) {
        if (!seq_set->contains(msg.sequence_number)) {
            continue;
        }

        switch (action->type) {
            case StoreAction::Type::FLAGS:
            case StoreAction::Type::FLAGS_SILENT:
                session.set_flags(msg.sequence_number, action->flags);
                break;
            case StoreAction::Type::PLUS_FLAGS:
            case StoreAction::Type::PLUS_FLAGS_SILENT:
                session.add_flags(msg.sequence_number, action->flags);
                break;
            case StoreAction::Type::MINUS_FLAGS:
            case StoreAction::Type::MINUS_FLAGS_SILENT:
                session.remove_flags(msg.sequence_number, action->flags);
                break;
        }

        if (!silent) {
            auto updated = session.get_message_by_sequence(msg.sequence_number);
            if (updated) {
                responses.push_back(response::untagged(
                    std::to_string(msg.sequence_number) + " FETCH (FLAGS " +
                    IMAPParser::format_flags(updated->flags) + ")"
                ));
            }
        }
    }

    responses.push_back(response::ok(cmd.tag, "STORE completed"));
    return responses;
}

std::vector<std::string> CommandHandler::handle_copy(IMAPSession& session, const Command& cmd) {
    if (session.state() != SessionState::SELECTED) {
        return {response::bad(cmd.tag, "No mailbox selected")};
    }

    size_t pos = 0;
    auto seq_str_opt = IMAPParser::parse_atom(cmd.arguments, pos);
    auto mailbox = IMAPParser::parse_string(cmd.arguments, pos);

    if (!seq_str_opt || !mailbox) {
        return {response::bad(cmd.tag, "Usage: COPY sequence mailbox")};
    }

    auto seq_set = IMAPParser::parse_sequence_set(*seq_str_opt);
    if (!seq_set) {
        return {response::bad(cmd.tag, "Invalid sequence set")};
    }

    auto* selected = session.selected_mailbox();
    if (!selected) {
        return {response::no(cmd.tag, "No mailbox selected")};
    }

    for (const auto& msg : session.messages()) {
        if (seq_set->contains(msg.sequence_number)) {
            session.maildir()->copy_message(msg.unique_id, selected->name, *mailbox);
        }
    }

    return {response::ok(cmd.tag, "COPY completed")};
}

std::vector<std::string> CommandHandler::handle_uid(IMAPSession& session, const Command& cmd) {
    if (session.state() != SessionState::SELECTED) {
        return {response::bad(cmd.tag, "No mailbox selected")};
    }

    // UID command wraps another command
    std::istringstream iss(cmd.arguments);
    std::string sub_cmd;
    std::string sub_args;
    iss >> sub_cmd;
    std::getline(iss >> std::ws, sub_args);

    std::transform(sub_cmd.begin(), sub_cmd.end(), sub_cmd.begin(), ::toupper);

    Command wrapped_cmd;
    wrapped_cmd.tag = cmd.tag;
    wrapped_cmd.name = sub_cmd;
    wrapped_cmd.arguments = sub_args;
    wrapped_cmd.type = Command::string_to_type(sub_cmd);

    if (sub_cmd == "SEARCH") {
        auto criteria = IMAPParser::parse_search_criteria(sub_args);
        auto results = session.search(criteria, true);  // Use UIDs

        std::ostringstream oss;
        oss << "SEARCH";
        for (uint32_t uid : results) {
            oss << " " << uid;
        }

        return {
            response::untagged(oss.str()),
            response::ok(cmd.tag, "UID SEARCH completed")
        };
    } else if (sub_cmd == "FETCH" || sub_cmd == "STORE" || sub_cmd == "COPY") {
        // These need UID-based sequence set handling
        // For simplicity, delegate to regular handlers
        return CommandHandler::instance().execute(session, wrapped_cmd);
    }

    return {response::bad(cmd.tag, "Unknown UID command")};
}

}  // namespace email::imap
