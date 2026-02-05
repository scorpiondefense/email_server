#pragma once

#include <string>
#include <vector>
#include <functional>
#include <unordered_map>

namespace email::imap {

class IMAPSession;

enum class CommandType {
    // Any state
    CAPABILITY,
    NOOP,
    LOGOUT,

    // Not authenticated state
    STARTTLS,
    AUTHENTICATE,
    LOGIN,

    // Authenticated state
    SELECT,
    EXAMINE,
    CREATE,
    DELETE,
    RENAME,
    SUBSCRIBE,
    UNSUBSCRIBE,
    LIST,
    LSUB,
    STATUS,
    APPEND,

    // Selected state
    CHECK,
    CLOSE,
    EXPUNGE,
    SEARCH,
    FETCH,
    STORE,
    COPY,
    UID,

    UNKNOWN
};

struct Command {
    std::string tag;
    CommandType type;
    std::string name;
    std::string arguments;

    static Command parse(const std::string& line);
    static CommandType string_to_type(const std::string& name);
    static std::string type_to_string(CommandType type);
};

class CommandHandler {
public:
    using Handler = std::function<std::vector<std::string>(IMAPSession&, const Command&)>;

    static CommandHandler& instance();

    void register_handler(CommandType type, Handler handler);
    std::vector<std::string> execute(IMAPSession& session, const Command& cmd);

    // Individual command handlers
    static std::vector<std::string> handle_capability(IMAPSession& session, const Command& cmd);
    static std::vector<std::string> handle_noop(IMAPSession& session, const Command& cmd);
    static std::vector<std::string> handle_logout(IMAPSession& session, const Command& cmd);
    static std::vector<std::string> handle_starttls(IMAPSession& session, const Command& cmd);
    static std::vector<std::string> handle_login(IMAPSession& session, const Command& cmd);
    static std::vector<std::string> handle_select(IMAPSession& session, const Command& cmd);
    static std::vector<std::string> handle_examine(IMAPSession& session, const Command& cmd);
    static std::vector<std::string> handle_create(IMAPSession& session, const Command& cmd);
    static std::vector<std::string> handle_delete(IMAPSession& session, const Command& cmd);
    static std::vector<std::string> handle_rename(IMAPSession& session, const Command& cmd);
    static std::vector<std::string> handle_list(IMAPSession& session, const Command& cmd);
    static std::vector<std::string> handle_lsub(IMAPSession& session, const Command& cmd);
    static std::vector<std::string> handle_status(IMAPSession& session, const Command& cmd);
    static std::vector<std::string> handle_append(IMAPSession& session, const Command& cmd);
    static std::vector<std::string> handle_check(IMAPSession& session, const Command& cmd);
    static std::vector<std::string> handle_close(IMAPSession& session, const Command& cmd);
    static std::vector<std::string> handle_expunge(IMAPSession& session, const Command& cmd);
    static std::vector<std::string> handle_search(IMAPSession& session, const Command& cmd);
    static std::vector<std::string> handle_fetch(IMAPSession& session, const Command& cmd);
    static std::vector<std::string> handle_store(IMAPSession& session, const Command& cmd);
    static std::vector<std::string> handle_copy(IMAPSession& session, const Command& cmd);
    static std::vector<std::string> handle_uid(IMAPSession& session, const Command& cmd);

private:
    CommandHandler();
    std::unordered_map<CommandType, Handler> handlers_;
};

// Response helpers
namespace response {
    inline std::string ok(const std::string& tag, const std::string& msg = "Completed") {
        return tag + " OK " + msg;
    }

    inline std::string no(const std::string& tag, const std::string& msg) {
        return tag + " NO " + msg;
    }

    inline std::string bad(const std::string& tag, const std::string& msg) {
        return tag + " BAD " + msg;
    }

    inline std::string untagged(const std::string& data) {
        return "* " + data;
    }

    inline std::string bye(const std::string& msg = "Logging out") {
        return "* BYE " + msg;
    }
}

}  // namespace email::imap
