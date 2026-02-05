#include <catch2/catch_test_macros.hpp>
#include "imap_commands.hpp"
#include "imap_parser.hpp"

using namespace email::imap;

TEST_CASE("IMAP command parsing", "[imap][commands]") {
    SECTION("Parse LOGIN command") {
        auto cmd = Command::parse("A001 LOGIN user password");
        REQUIRE(cmd.tag == "A001");
        REQUIRE(cmd.type == CommandType::LOGIN);
        REQUIRE(cmd.name == "LOGIN");
        REQUIRE(cmd.arguments == "user password");
    }

    SECTION("Parse SELECT command") {
        auto cmd = Command::parse("A002 SELECT INBOX");
        REQUIRE(cmd.tag == "A002");
        REQUIRE(cmd.type == CommandType::SELECT);
        REQUIRE(cmd.arguments == "INBOX");
    }

    SECTION("Parse FETCH command") {
        auto cmd = Command::parse("A003 FETCH 1:* (FLAGS UID)");
        REQUIRE(cmd.tag == "A003");
        REQUIRE(cmd.type == CommandType::FETCH);
        REQUIRE(cmd.arguments == "1:* (FLAGS UID)");
    }

    SECTION("Parse LOGOUT command") {
        auto cmd = Command::parse("A004 LOGOUT");
        REQUIRE(cmd.tag == "A004");
        REQUIRE(cmd.type == CommandType::LOGOUT);
    }

    SECTION("Parse unknown command") {
        auto cmd = Command::parse("A005 INVALID");
        REQUIRE(cmd.tag == "A005");
        REQUIRE(cmd.type == CommandType::UNKNOWN);
    }

    SECTION("Case insensitive command") {
        auto cmd1 = Command::parse("a001 login user pass");
        auto cmd2 = Command::parse("a001 LOGIN user pass");
        REQUIRE(cmd1.type == CommandType::LOGIN);
        REQUIRE(cmd2.type == CommandType::LOGIN);
    }
}

TEST_CASE("IMAP parser - sequence set", "[imap][parser]") {
    SECTION("Single message") {
        auto set = IMAPParser::parse_sequence_set("1");
        REQUIRE(set.has_value());
        REQUIRE(set->contains(1));
        REQUIRE_FALSE(set->contains(2));
    }

    SECTION("Range") {
        auto set = IMAPParser::parse_sequence_set("1:5");
        REQUIRE(set.has_value());
        REQUIRE(set->contains(1));
        REQUIRE(set->contains(3));
        REQUIRE(set->contains(5));
        REQUIRE_FALSE(set->contains(6));
    }

    SECTION("Multiple ranges") {
        auto set = IMAPParser::parse_sequence_set("1:3,5,7:9");
        REQUIRE(set.has_value());
        REQUIRE(set->contains(1));
        REQUIRE(set->contains(2));
        REQUIRE(set->contains(3));
        REQUIRE_FALSE(set->contains(4));
        REQUIRE(set->contains(5));
        REQUIRE_FALSE(set->contains(6));
        REQUIRE(set->contains(7));
        REQUIRE(set->contains(8));
        REQUIRE(set->contains(9));
    }

    SECTION("Wildcard") {
        auto set = IMAPParser::parse_sequence_set("1:*");
        REQUIRE(set.has_value());
        REQUIRE(set->contains(1));
        REQUIRE(set->contains(1000000));
    }
}

TEST_CASE("IMAP parser - fetch items", "[imap][parser]") {
    SECTION("Single item") {
        auto items = IMAPParser::parse_fetch_items("FLAGS");
        REQUIRE(items.size() == 1);
        REQUIRE(items[0].type == FetchItem::Type::FLAGS);
    }

    SECTION("Multiple items in parentheses") {
        auto items = IMAPParser::parse_fetch_items("(FLAGS UID RFC822.SIZE)");
        REQUIRE(items.size() == 3);
        REQUIRE(items[0].type == FetchItem::Type::FLAGS);
        REQUIRE(items[1].type == FetchItem::Type::UID);
        REQUIRE(items[2].type == FetchItem::Type::RFC822_SIZE);
    }

    SECTION("BODY with section") {
        auto items = IMAPParser::parse_fetch_items("BODY[HEADER]");
        REQUIRE(items.size() == 1);
        REQUIRE(items[0].type == FetchItem::Type::BODY);
        REQUIRE(items[0].section == "HEADER");
    }

    SECTION("BODY.PEEK with section") {
        auto items = IMAPParser::parse_fetch_items("BODY.PEEK[TEXT]");
        REQUIRE(items.size() == 1);
        REQUIRE(items[0].type == FetchItem::Type::BODY_PEEK);
        REQUIRE(items[0].section == "TEXT");
    }

    SECTION("ALL macro") {
        auto items = IMAPParser::parse_fetch_items("ALL");
        REQUIRE(items.size() == 1);
        REQUIRE(items[0].type == FetchItem::Type::ALL);
    }
}

TEST_CASE("IMAP parser - search criteria", "[imap][parser]") {
    SECTION("Simple flag search") {
        auto criteria = IMAPParser::parse_search_criteria("UNSEEN");
        REQUIRE(criteria.size() == 1);
        REQUIRE(criteria[0].type == SearchCriteria::Type::UNSEEN);
    }

    SECTION("Multiple criteria") {
        auto criteria = IMAPParser::parse_search_criteria("UNSEEN FLAGGED");
        REQUIRE(criteria.size() == 2);
        REQUIRE(criteria[0].type == SearchCriteria::Type::UNSEEN);
        REQUIRE(criteria[1].type == SearchCriteria::Type::FLAGGED);
    }

    SECTION("Search with value") {
        auto criteria = IMAPParser::parse_search_criteria("FROM john@example.com");
        REQUIRE(criteria.size() == 1);
        REQUIRE(criteria[0].type == SearchCriteria::Type::FROM);
        REQUIRE(criteria[0].value == "john@example.com");
    }

    SECTION("Size search") {
        auto criteria = IMAPParser::parse_search_criteria("LARGER 1024");
        REQUIRE(criteria.size() == 1);
        REQUIRE(criteria[0].type == SearchCriteria::Type::LARGER);
        REQUIRE(criteria[0].value == "1024");
    }
}

TEST_CASE("IMAP parser - store action", "[imap][parser]") {
    SECTION("FLAGS") {
        auto action = IMAPParser::parse_store_action("FLAGS (\\Seen)");
        REQUIRE(action.has_value());
        REQUIRE(action->type == StoreAction::Type::FLAGS);
        REQUIRE(action->flags.count("\\Seen") == 1);
    }

    SECTION("+FLAGS") {
        auto action = IMAPParser::parse_store_action("+FLAGS (\\Deleted)");
        REQUIRE(action.has_value());
        REQUIRE(action->type == StoreAction::Type::PLUS_FLAGS);
        REQUIRE(action->flags.count("\\Deleted") == 1);
    }

    SECTION("-FLAGS.SILENT") {
        auto action = IMAPParser::parse_store_action("-FLAGS.SILENT (\\Seen \\Flagged)");
        REQUIRE(action.has_value());
        REQUIRE(action->type == StoreAction::Type::MINUS_FLAGS_SILENT);
        REQUIRE(action->flags.size() == 2);
    }
}

TEST_CASE("IMAP parser - string parsing", "[imap][parser]") {
    SECTION("Quoted string") {
        size_t pos = 0;
        auto str = IMAPParser::parse_string("\"hello world\"", pos);
        REQUIRE(str.has_value());
        REQUIRE(*str == "hello world");
    }

    SECTION("Atom") {
        size_t pos = 0;
        auto str = IMAPParser::parse_string("INBOX", pos);
        REQUIRE(str.has_value());
        REQUIRE(*str == "INBOX");
    }

    SECTION("Quote string utility") {
        REQUIRE(IMAPParser::quote_string("simple") == "simple");
        REQUIRE(IMAPParser::quote_string("with space") == "\"with space\"");
        REQUIRE(IMAPParser::quote_string("with\"quote") == "\"with\\\"quote\"");
    }
}

TEST_CASE("IMAP parser - flag formatting", "[imap][parser]") {
    SECTION("Format empty flags") {
        std::set<std::string> flags;
        REQUIRE(IMAPParser::format_flags(flags) == "()");
    }

    SECTION("Format single flag") {
        std::set<std::string> flags{"\\Seen"};
        REQUIRE(IMAPParser::format_flags(flags) == "(\\Seen)");
    }

    SECTION("Format multiple flags") {
        std::set<std::string> flags{"\\Answered", "\\Seen"};
        auto formatted = IMAPParser::format_flags(flags);
        // Set order may vary, so just check content
        REQUIRE(formatted.find("\\Answered") != std::string::npos);
        REQUIRE(formatted.find("\\Seen") != std::string::npos);
    }

    SECTION("Parse flag list") {
        auto flags = IMAPParser::parse_flag_list("(\\Seen \\Answered \\Flagged)");
        REQUIRE(flags.size() == 3);
        REQUIRE(flags.count("\\Seen") == 1);
        REQUIRE(flags.count("\\Answered") == 1);
        REQUIRE(flags.count("\\Flagged") == 1);
    }
}

TEST_CASE("IMAP responses", "[imap][responses]") {
    SECTION("OK response") {
        REQUIRE(response::ok("A001", "Success") == "A001 OK Success");
    }

    SECTION("NO response") {
        REQUIRE(response::no("A001", "Failed") == "A001 NO Failed");
    }

    SECTION("BAD response") {
        REQUIRE(response::bad("A001", "Invalid") == "A001 BAD Invalid");
    }

    SECTION("Untagged response") {
        REQUIRE(response::untagged("5 EXISTS") == "* 5 EXISTS");
    }

    SECTION("BYE response") {
        REQUIRE(response::bye("Goodbye") == "* BYE Goodbye");
    }
}
