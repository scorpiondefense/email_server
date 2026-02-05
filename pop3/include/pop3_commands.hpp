#pragma once

#include <string>
#include <vector>
#include <functional>
#include <unordered_map>
#include <optional>

namespace email::pop3 {

class POP3Session;

enum class CommandType {
    USER,
    PASS,
    STAT,
    LIST,
    RETR,
    DELE,
    NOOP,
    RSET,
    QUIT,
    TOP,
    UIDL,
    CAPA,
    STLS,
    AUTH,
    APOP,
    UNKNOWN
};

struct Command {
    CommandType type;
    std::string name;
    std::string argument;
    std::vector<std::string> args;

    static Command parse(const std::string& line);
    static CommandType string_to_type(const std::string& name);
    static std::string type_to_string(CommandType type);
};

class CommandHandler {
public:
    using Handler = std::function<std::string(POP3Session&, const Command&)>;

    static CommandHandler& instance();

    void register_handler(CommandType type, Handler handler);
    std::string execute(POP3Session& session, const Command& cmd);

    // Individual command handlers
    static std::string handle_user(POP3Session& session, const Command& cmd);
    static std::string handle_pass(POP3Session& session, const Command& cmd);
    static std::string handle_stat(POP3Session& session, const Command& cmd);
    static std::string handle_list(POP3Session& session, const Command& cmd);
    static std::string handle_retr(POP3Session& session, const Command& cmd);
    static std::string handle_dele(POP3Session& session, const Command& cmd);
    static std::string handle_noop(POP3Session& session, const Command& cmd);
    static std::string handle_rset(POP3Session& session, const Command& cmd);
    static std::string handle_quit(POP3Session& session, const Command& cmd);
    static std::string handle_top(POP3Session& session, const Command& cmd);
    static std::string handle_uidl(POP3Session& session, const Command& cmd);
    static std::string handle_capa(POP3Session& session, const Command& cmd);
    static std::string handle_stls(POP3Session& session, const Command& cmd);
    static std::string handle_auth(POP3Session& session, const Command& cmd);

private:
    CommandHandler();
    std::unordered_map<CommandType, Handler> handlers_;
};

// Response helpers
namespace response {
    constexpr const char* OK = "+OK";
    constexpr const char* ERR = "-ERR";

    inline std::string ok(const std::string& msg = "") {
        return msg.empty() ? std::string(OK) : std::string(OK) + " " + msg;
    }

    inline std::string err(const std::string& msg = "") {
        return msg.empty() ? std::string(ERR) : std::string(ERR) + " " + msg;
    }
}

}  // namespace email::pop3
