#pragma once

#include <string>
#include <vector>
#include <functional>
#include <unordered_map>
#include <optional>

namespace email::smtp {

class SMTPSession;

enum class CommandType {
    HELO,
    EHLO,
    MAIL,      // MAIL FROM:
    RCPT,      // RCPT TO:
    DATA,
    RSET,
    NOOP,
    QUIT,
    VRFY,
    AUTH,
    STARTTLS,
    HELP,
    UNKNOWN
};

struct Command {
    CommandType type;
    std::string name;
    std::string argument;

    static Command parse(const std::string& line);
    static CommandType string_to_type(const std::string& name);
    static std::string type_to_string(CommandType type);
};

class CommandHandler {
public:
    using Handler = std::function<std::string(SMTPSession&, const Command&)>;

    static CommandHandler& instance();

    void register_handler(CommandType type, Handler handler);
    std::string execute(SMTPSession& session, const Command& cmd);

    // Individual command handlers
    static std::string handle_helo(SMTPSession& session, const Command& cmd);
    static std::string handle_ehlo(SMTPSession& session, const Command& cmd);
    static std::string handle_mail(SMTPSession& session, const Command& cmd);
    static std::string handle_rcpt(SMTPSession& session, const Command& cmd);
    static std::string handle_data(SMTPSession& session, const Command& cmd);
    static std::string handle_rset(SMTPSession& session, const Command& cmd);
    static std::string handle_noop(SMTPSession& session, const Command& cmd);
    static std::string handle_quit(SMTPSession& session, const Command& cmd);
    static std::string handle_vrfy(SMTPSession& session, const Command& cmd);
    static std::string handle_auth(SMTPSession& session, const Command& cmd);
    static std::string handle_starttls(SMTPSession& session, const Command& cmd);
    static std::string handle_help(SMTPSession& session, const Command& cmd);

private:
    CommandHandler();
    std::unordered_map<CommandType, Handler> handlers_;
};

// SMTP Reply codes and messages
namespace reply {
    // 2xx - Positive Completion
    constexpr int SYSTEM_STATUS = 211;
    constexpr int HELP = 214;
    constexpr int SERVICE_READY = 220;
    constexpr int SERVICE_CLOSING = 221;
    constexpr int AUTH_SUCCESS = 235;
    constexpr int OK = 250;
    constexpr int USER_NOT_LOCAL = 251;
    constexpr int CANNOT_VRFY = 252;

    // 3xx - Positive Intermediate
    constexpr int AUTH_CONTINUE = 334;
    constexpr int START_MAIL_INPUT = 354;

    // 4xx - Transient Negative
    constexpr int SERVICE_NOT_AVAILABLE = 421;
    constexpr int MAILBOX_BUSY = 450;
    constexpr int LOCAL_ERROR = 451;
    constexpr int INSUFFICIENT_STORAGE = 452;
    constexpr int TEMP_AUTH_FAILURE = 454;

    // 5xx - Permanent Negative
    constexpr int SYNTAX_ERROR = 500;
    constexpr int SYNTAX_ERROR_PARAMS = 501;
    constexpr int COMMAND_NOT_IMPLEMENTED = 502;
    constexpr int BAD_SEQUENCE = 503;
    constexpr int PARAM_NOT_IMPLEMENTED = 504;
    constexpr int MAILBOX_NOT_FOUND = 550;
    constexpr int USER_NOT_LOCAL_ERROR = 551;
    constexpr int EXCEEDED_STORAGE = 552;
    constexpr int MAILBOX_NAME_INVALID = 553;
    constexpr int TRANSACTION_FAILED = 554;
    constexpr int AUTH_REQUIRED = 530;
    constexpr int AUTH_INVALID = 535;

    inline std::string make(int code, const std::string& message) {
        return std::to_string(code) + " " + message;
    }

    inline std::string make_multi(int code, const std::vector<std::string>& lines) {
        std::string result;
        for (size_t i = 0; i < lines.size(); ++i) {
            if (i == lines.size() - 1) {
                result += std::to_string(code) + " " + lines[i];
            } else {
                result += std::to_string(code) + "-" + lines[i] + "\r\n";
            }
        }
        return result;
    }
}

// Email address parsing
struct EmailAddress {
    std::string local_part;
    std::string domain;
    std::string full_address;

    static std::optional<EmailAddress> parse(const std::string& str);
    std::string to_string() const { return local_part + "@" + domain; }
};

}  // namespace email::smtp
