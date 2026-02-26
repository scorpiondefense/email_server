#pragma once

#include <chrono>
#include <cstdint>
#include <optional>
#include <set>
#include <string>
#include <variant>
#include <vector>

namespace email::imap {

// IMAP data types
struct IMAPList {
    std::vector<std::variant<std::string, IMAPList>> items;
};

struct SequenceSet {
    struct Range {
        uint32_t start;
        uint32_t end;  // 0 means * (all)
    };
    std::vector<Range> ranges;

    bool contains(uint32_t num) const;
    static SequenceSet parse(const std::string& str);
};

struct FetchItem {
    enum class Type {
        ALL,
        FAST,
        FULL,
        ENVELOPE,
        FLAGS,
        INTERNALDATE,
        RFC822,
        RFC822_HEADER,
        RFC822_SIZE,
        RFC822_TEXT,
        BODY,
        BODY_PEEK,
        BODYSTRUCTURE,
        UID
    };

    Type type;
    std::string section;      // For BODY[section]
    std::optional<std::pair<size_t, size_t>> partial;  // <start.count>
};

struct SearchCriteria {
    enum class Type {
        ALL,
        ANSWERED,
        DELETED,
        DRAFT,
        FLAGGED,
        NEW,
        OLD,
        RECENT,
        SEEN,
        UNANSWERED,
        UNDELETED,
        UNDRAFT,
        UNFLAGGED,
        UNSEEN,
        BCC,
        BEFORE,
        BODY,
        CC,
        FROM,
        HEADER,
        KEYWORD,
        LARGER,
        NOT,
        ON,
        OR,
        SENTBEFORE,
        SENTON,
        SENTSINCE,
        SINCE,
        SMALLER,
        SUBJECT,
        TEXT,
        TO,
        UID,
        UNKEYWORD
    };

    Type type;
    std::string value;
    std::string header_name;  // For HEADER searches
    std::vector<SearchCriteria> sub_criteria;  // For NOT, OR
};

struct StoreAction {
    enum class Type {
        FLAGS,
        FLAGS_SILENT,
        PLUS_FLAGS,
        PLUS_FLAGS_SILENT,
        MINUS_FLAGS,
        MINUS_FLAGS_SILENT
    };

    Type type;
    std::set<std::string> flags;
};

class IMAPParser {
public:
    // Parse a complete IMAP command line
    static bool parse_command(const std::string& line, std::string& tag,
                              std::string& command, std::string& arguments);

    // Parse specific argument types
    static std::optional<SequenceSet> parse_sequence_set(const std::string& str);
    static std::vector<FetchItem> parse_fetch_items(const std::string& str);
    static std::vector<SearchCriteria> parse_search_criteria(const std::string& str);
    static std::optional<StoreAction> parse_store_action(const std::string& str);

    // Parse IMAP literals and quoted strings
    static std::optional<std::string> parse_string(const std::string& str, size_t& pos);
    static std::optional<std::string> parse_atom(const std::string& str, size_t& pos);
    static std::optional<IMAPList> parse_list(const std::string& str, size_t& pos);

    // Utility functions
    static std::string quote_string(const std::string& str);
    static std::string format_flags(const std::set<std::string>& flags);
    static std::set<std::string> parse_flag_list(const std::string& str);

    // Date parsing/formatting
    static std::optional<std::chrono::system_clock::time_point> parse_date(const std::string& str);
    static std::string format_date(std::chrono::system_clock::time_point tp);
    static std::string format_internal_date(std::chrono::system_clock::time_point tp);

private:
    static void skip_whitespace(const std::string& str, size_t& pos);
    static bool is_atom_char(char c);
    static bool is_list_wildcard(char c);
};

}  // namespace email::imap
