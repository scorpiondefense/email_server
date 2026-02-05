#include "imap_parser.hpp"
#include <algorithm>
#include <sstream>
#include <cctype>
#include <iomanip>
#include <ctime>

namespace email::imap {

bool SequenceSet::contains(uint32_t num) const {
    for (const auto& range : ranges) {
        uint32_t end = (range.end == 0) ? UINT32_MAX : range.end;
        if (num >= range.start && num <= end) {
            return true;
        }
    }
    return false;
}

SequenceSet SequenceSet::parse(const std::string& str) {
    SequenceSet set;

    std::istringstream iss(str);
    std::string token;

    while (std::getline(iss, token, ',')) {
        Range range;

        auto colon_pos = token.find(':');
        if (colon_pos != std::string::npos) {
            std::string start_str = token.substr(0, colon_pos);
            std::string end_str = token.substr(colon_pos + 1);

            range.start = (start_str == "*") ? UINT32_MAX : std::stoul(start_str);
            range.end = (end_str == "*") ? 0 : std::stoul(end_str);

            // Normalize: start should be <= end
            if (range.end != 0 && range.start > range.end) {
                std::swap(range.start, range.end);
            }
        } else {
            if (token == "*") {
                range.start = UINT32_MAX;
                range.end = 0;
            } else {
                range.start = std::stoul(token);
                range.end = range.start;
            }
        }

        set.ranges.push_back(range);
    }

    return set;
}

bool IMAPParser::parse_command(const std::string& line, std::string& tag,
                               std::string& command, std::string& arguments) {
    if (line.empty()) {
        return false;
    }

    std::istringstream iss(line);

    // Read tag
    iss >> tag;
    if (tag.empty()) {
        return false;
    }

    // Read command
    iss >> command;
    if (command.empty()) {
        return false;
    }

    // Convert command to uppercase
    std::transform(command.begin(), command.end(), command.begin(), ::toupper);

    // Rest is arguments
    std::getline(iss >> std::ws, arguments);

    return true;
}

std::optional<SequenceSet> IMAPParser::parse_sequence_set(const std::string& str) {
    if (str.empty()) {
        return std::nullopt;
    }
    return SequenceSet::parse(str);
}

std::vector<FetchItem> IMAPParser::parse_fetch_items(const std::string& str) {
    std::vector<FetchItem> items;

    std::string s = str;
    // Remove parentheses if present
    if (!s.empty() && s.front() == '(') {
        s = s.substr(1);
        if (!s.empty() && s.back() == ')') {
            s.pop_back();
        }
    }

    std::istringstream iss(s);
    std::string token;

    while (iss >> token) {
        std::transform(token.begin(), token.end(), token.begin(), ::toupper);

        FetchItem item;

        if (token == "ALL") {
            item.type = FetchItem::Type::ALL;
        } else if (token == "FAST") {
            item.type = FetchItem::Type::FAST;
        } else if (token == "FULL") {
            item.type = FetchItem::Type::FULL;
        } else if (token == "ENVELOPE") {
            item.type = FetchItem::Type::ENVELOPE;
        } else if (token == "FLAGS") {
            item.type = FetchItem::Type::FLAGS;
        } else if (token == "INTERNALDATE") {
            item.type = FetchItem::Type::INTERNALDATE;
        } else if (token == "RFC822") {
            item.type = FetchItem::Type::RFC822;
        } else if (token == "RFC822.HEADER") {
            item.type = FetchItem::Type::RFC822_HEADER;
        } else if (token == "RFC822.SIZE") {
            item.type = FetchItem::Type::RFC822_SIZE;
        } else if (token == "RFC822.TEXT") {
            item.type = FetchItem::Type::RFC822_TEXT;
        } else if (token.find("BODY.PEEK") == 0) {
            item.type = FetchItem::Type::BODY_PEEK;
            // Parse section if present
            auto bracket_pos = token.find('[');
            if (bracket_pos != std::string::npos) {
                auto end_bracket = token.find(']', bracket_pos);
                if (end_bracket != std::string::npos) {
                    item.section = token.substr(bracket_pos + 1, end_bracket - bracket_pos - 1);
                }
            }
        } else if (token.find("BODY") == 0) {
            item.type = FetchItem::Type::BODY;
            auto bracket_pos = token.find('[');
            if (bracket_pos != std::string::npos) {
                auto end_bracket = token.find(']', bracket_pos);
                if (end_bracket != std::string::npos) {
                    item.section = token.substr(bracket_pos + 1, end_bracket - bracket_pos - 1);
                }
            }
        } else if (token == "BODYSTRUCTURE") {
            item.type = FetchItem::Type::BODYSTRUCTURE;
        } else if (token == "UID") {
            item.type = FetchItem::Type::UID;
        } else {
            continue;  // Unknown item, skip
        }

        items.push_back(item);
    }

    return items;
}

std::vector<SearchCriteria> IMAPParser::parse_search_criteria(const std::string& str) {
    std::vector<SearchCriteria> criteria;

    std::istringstream iss(str);
    std::string token;

    while (iss >> token) {
        std::transform(token.begin(), token.end(), token.begin(), ::toupper);

        SearchCriteria crit;

        if (token == "ALL") {
            crit.type = SearchCriteria::Type::ALL;
        } else if (token == "ANSWERED") {
            crit.type = SearchCriteria::Type::ANSWERED;
        } else if (token == "DELETED") {
            crit.type = SearchCriteria::Type::DELETED;
        } else if (token == "DRAFT") {
            crit.type = SearchCriteria::Type::DRAFT;
        } else if (token == "FLAGGED") {
            crit.type = SearchCriteria::Type::FLAGGED;
        } else if (token == "NEW") {
            crit.type = SearchCriteria::Type::NEW;
        } else if (token == "OLD") {
            crit.type = SearchCriteria::Type::OLD;
        } else if (token == "RECENT") {
            crit.type = SearchCriteria::Type::RECENT;
        } else if (token == "SEEN") {
            crit.type = SearchCriteria::Type::SEEN;
        } else if (token == "UNANSWERED") {
            crit.type = SearchCriteria::Type::UNANSWERED;
        } else if (token == "UNDELETED") {
            crit.type = SearchCriteria::Type::UNDELETED;
        } else if (token == "UNDRAFT") {
            crit.type = SearchCriteria::Type::UNDRAFT;
        } else if (token == "UNFLAGGED") {
            crit.type = SearchCriteria::Type::UNFLAGGED;
        } else if (token == "UNSEEN") {
            crit.type = SearchCriteria::Type::UNSEEN;
        } else if (token == "FROM") {
            crit.type = SearchCriteria::Type::FROM;
            iss >> crit.value;
        } else if (token == "TO") {
            crit.type = SearchCriteria::Type::TO;
            iss >> crit.value;
        } else if (token == "CC") {
            crit.type = SearchCriteria::Type::CC;
            iss >> crit.value;
        } else if (token == "BCC") {
            crit.type = SearchCriteria::Type::BCC;
            iss >> crit.value;
        } else if (token == "SUBJECT") {
            crit.type = SearchCriteria::Type::SUBJECT;
            iss >> crit.value;
        } else if (token == "BODY") {
            crit.type = SearchCriteria::Type::BODY;
            iss >> crit.value;
        } else if (token == "TEXT") {
            crit.type = SearchCriteria::Type::TEXT;
            iss >> crit.value;
        } else if (token == "LARGER") {
            crit.type = SearchCriteria::Type::LARGER;
            iss >> crit.value;
        } else if (token == "SMALLER") {
            crit.type = SearchCriteria::Type::SMALLER;
            iss >> crit.value;
        } else if (token == "BEFORE") {
            crit.type = SearchCriteria::Type::BEFORE;
            iss >> crit.value;
        } else if (token == "ON") {
            crit.type = SearchCriteria::Type::ON;
            iss >> crit.value;
        } else if (token == "SINCE") {
            crit.type = SearchCriteria::Type::SINCE;
            iss >> crit.value;
        } else if (token == "UID") {
            crit.type = SearchCriteria::Type::UID;
            iss >> crit.value;
        } else {
            continue;  // Unknown criteria
        }

        criteria.push_back(crit);
    }

    return criteria;
}

std::optional<StoreAction> IMAPParser::parse_store_action(const std::string& str) {
    std::istringstream iss(str);
    std::string action;
    iss >> action;

    std::transform(action.begin(), action.end(), action.begin(), ::toupper);

    StoreAction store;

    if (action == "FLAGS" || action == "FLAGS.SILENT") {
        store.type = (action == "FLAGS") ? StoreAction::Type::FLAGS : StoreAction::Type::FLAGS_SILENT;
    } else if (action == "+FLAGS" || action == "+FLAGS.SILENT") {
        store.type = (action == "+FLAGS") ? StoreAction::Type::PLUS_FLAGS : StoreAction::Type::PLUS_FLAGS_SILENT;
    } else if (action == "-FLAGS" || action == "-FLAGS.SILENT") {
        store.type = (action == "-FLAGS") ? StoreAction::Type::MINUS_FLAGS : StoreAction::Type::MINUS_FLAGS_SILENT;
    } else {
        return std::nullopt;
    }

    // Parse flag list
    std::string rest;
    std::getline(iss >> std::ws, rest);
    store.flags = parse_flag_list(rest);

    return store;
}

std::optional<std::string> IMAPParser::parse_string(const std::string& str, size_t& pos) {
    skip_whitespace(str, pos);
    if (pos >= str.length()) {
        return std::nullopt;
    }

    if (str[pos] == '"') {
        // Quoted string
        pos++;
        std::string result;
        while (pos < str.length() && str[pos] != '"') {
            if (str[pos] == '\\' && pos + 1 < str.length()) {
                pos++;
            }
            result += str[pos++];
        }
        if (pos < str.length()) pos++;  // Skip closing quote
        return result;
    } else if (str[pos] == '{') {
        // Literal (we don't support this in simple parsing)
        return std::nullopt;
    } else {
        // Atom
        return parse_atom(str, pos);
    }
}

std::optional<std::string> IMAPParser::parse_atom(const std::string& str, size_t& pos) {
    skip_whitespace(str, pos);

    std::string result;
    while (pos < str.length() && is_atom_char(str[pos])) {
        result += str[pos++];
    }

    return result.empty() ? std::nullopt : std::optional<std::string>(result);
}

std::optional<IMAPList> IMAPParser::parse_list(const std::string& str, size_t& pos) {
    skip_whitespace(str, pos);
    if (pos >= str.length() || str[pos] != '(') {
        return std::nullopt;
    }

    pos++;  // Skip (
    IMAPList list;

    while (pos < str.length() && str[pos] != ')') {
        skip_whitespace(str, pos);
        if (pos >= str.length()) break;

        if (str[pos] == '(') {
            auto sublist = parse_list(str, pos);
            if (sublist) {
                list.items.push_back(*sublist);
            }
        } else {
            auto item = parse_string(str, pos);
            if (item) {
                list.items.push_back(*item);
            }
        }
    }

    if (pos < str.length()) pos++;  // Skip )
    return list;
}

std::string IMAPParser::quote_string(const std::string& str) {
    bool needs_quotes = false;

    for (char c : str) {
        if (!is_atom_char(c)) {
            needs_quotes = true;
            break;
        }
    }

    if (!needs_quotes && !str.empty()) {
        return str;
    }

    std::string result = "\"";
    for (char c : str) {
        if (c == '"' || c == '\\') {
            result += '\\';
        }
        result += c;
    }
    result += "\"";

    return result;
}

std::string IMAPParser::format_flags(const std::set<std::string>& flags) {
    std::string result = "(";
    bool first = true;

    for (const auto& flag : flags) {
        if (!first) result += " ";
        result += flag;
        first = false;
    }

    result += ")";
    return result;
}

std::set<std::string> IMAPParser::parse_flag_list(const std::string& str) {
    std::set<std::string> flags;

    std::string s = str;
    // Remove parentheses
    if (!s.empty() && s.front() == '(') {
        s = s.substr(1);
        if (!s.empty() && s.back() == ')') {
            s.pop_back();
        }
    }

    std::istringstream iss(s);
    std::string flag;
    while (iss >> flag) {
        flags.insert(flag);
    }

    return flags;
}

std::optional<std::chrono::system_clock::time_point> IMAPParser::parse_date(const std::string& str) {
    std::tm tm = {};
    std::istringstream iss(str);

    // Try IMAP date format: DD-Mon-YYYY
    iss >> std::get_time(&tm, "%d-%b-%Y");
    if (!iss.fail()) {
        return std::chrono::system_clock::from_time_t(std::mktime(&tm));
    }

    return std::nullopt;
}

std::string IMAPParser::format_date(std::chrono::system_clock::time_point tp) {
    auto time_t_val = std::chrono::system_clock::to_time_t(tp);
    std::tm tm;
#ifdef _WIN32
    localtime_s(&tm, &time_t_val);
#else
    localtime_r(&time_t_val, &tm);
#endif

    std::ostringstream oss;
    oss << std::put_time(&tm, "%d-%b-%Y");
    return oss.str();
}

std::string IMAPParser::format_internal_date(std::chrono::system_clock::time_point tp) {
    auto time_t_val = std::chrono::system_clock::to_time_t(tp);
    std::tm tm;
#ifdef _WIN32
    localtime_s(&tm, &time_t_val);
#else
    localtime_r(&time_t_val, &tm);
#endif

    std::ostringstream oss;
    oss << "\"" << std::put_time(&tm, "%d-%b-%Y %H:%M:%S %z") << "\"";
    return oss.str();
}

void IMAPParser::skip_whitespace(const std::string& str, size_t& pos) {
    while (pos < str.length() && std::isspace(str[pos])) {
        pos++;
    }
}

bool IMAPParser::is_atom_char(char c) {
    // IMAP atom-char excludes: ( ) { SP CTL list-wildcards quoted-specials resp-specials
    if (std::isspace(c) || std::iscntrl(c)) return false;
    if (c == '(' || c == ')' || c == '{') return false;
    if (c == '"' || c == '\\') return false;
    if (c == '%' || c == '*') return false;
    if (c == '[' || c == ']') return false;
    return true;
}

bool IMAPParser::is_list_wildcard(char c) {
    return c == '%' || c == '*';
}

}  // namespace email::imap
