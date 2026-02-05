#include <catch2/catch_test_macros.hpp>
#include "pop3_commands.hpp"

using namespace email::pop3;

TEST_CASE("POP3 command parsing", "[pop3][commands]") {
    SECTION("Parse USER command") {
        auto cmd = Command::parse("USER john@example.com");
        REQUIRE(cmd.type == CommandType::USER);
        REQUIRE(cmd.name == "USER");
        REQUIRE(cmd.argument == "john@example.com");
    }

    SECTION("Parse PASS command") {
        auto cmd = Command::parse("PASS secret123");
        REQUIRE(cmd.type == CommandType::PASS);
        REQUIRE(cmd.name == "PASS");
        REQUIRE(cmd.argument == "secret123");
    }

    SECTION("Parse STAT command") {
        auto cmd = Command::parse("STAT");
        REQUIRE(cmd.type == CommandType::STAT);
        REQUIRE(cmd.argument.empty());
    }

    SECTION("Parse LIST command without argument") {
        auto cmd = Command::parse("LIST");
        REQUIRE(cmd.type == CommandType::LIST);
        REQUIRE(cmd.argument.empty());
    }

    SECTION("Parse LIST command with argument") {
        auto cmd = Command::parse("LIST 1");
        REQUIRE(cmd.type == CommandType::LIST);
        REQUIRE(cmd.argument == "1");
    }

    SECTION("Parse RETR command") {
        auto cmd = Command::parse("RETR 5");
        REQUIRE(cmd.type == CommandType::RETR);
        REQUIRE(cmd.argument == "5");
    }

    SECTION("Parse DELE command") {
        auto cmd = Command::parse("DELE 3");
        REQUIRE(cmd.type == CommandType::DELE);
        REQUIRE(cmd.argument == "3");
    }

    SECTION("Parse NOOP command") {
        auto cmd = Command::parse("NOOP");
        REQUIRE(cmd.type == CommandType::NOOP);
    }

    SECTION("Parse RSET command") {
        auto cmd = Command::parse("RSET");
        REQUIRE(cmd.type == CommandType::RSET);
    }

    SECTION("Parse QUIT command") {
        auto cmd = Command::parse("QUIT");
        REQUIRE(cmd.type == CommandType::QUIT);
    }

    SECTION("Parse TOP command") {
        auto cmd = Command::parse("TOP 1 10");
        REQUIRE(cmd.type == CommandType::TOP);
        REQUIRE(cmd.args.size() == 2);
        REQUIRE(cmd.args[0] == "1");
        REQUIRE(cmd.args[1] == "10");
    }

    SECTION("Parse UIDL command") {
        auto cmd = Command::parse("UIDL");
        REQUIRE(cmd.type == CommandType::UIDL);
    }

    SECTION("Parse CAPA command") {
        auto cmd = Command::parse("CAPA");
        REQUIRE(cmd.type == CommandType::CAPA);
    }

    SECTION("Parse STLS command") {
        auto cmd = Command::parse("STLS");
        REQUIRE(cmd.type == CommandType::STLS);
    }

    SECTION("Parse unknown command") {
        auto cmd = Command::parse("INVALID");
        REQUIRE(cmd.type == CommandType::UNKNOWN);
    }

    SECTION("Case insensitive parsing") {
        auto cmd1 = Command::parse("user john@example.com");
        auto cmd2 = Command::parse("User john@example.com");
        auto cmd3 = Command::parse("USER john@example.com");

        REQUIRE(cmd1.type == CommandType::USER);
        REQUIRE(cmd2.type == CommandType::USER);
        REQUIRE(cmd3.type == CommandType::USER);
    }

    SECTION("Empty command") {
        auto cmd = Command::parse("");
        REQUIRE(cmd.type == CommandType::UNKNOWN);
    }
}

TEST_CASE("POP3 responses", "[pop3][responses]") {
    SECTION("OK response without message") {
        REQUIRE(response::ok() == "+OK");
    }

    SECTION("OK response with message") {
        REQUIRE(response::ok("Success") == "+OK Success");
    }

    SECTION("ERR response without message") {
        REQUIRE(response::err() == "-ERR");
    }

    SECTION("ERR response with message") {
        REQUIRE(response::err("Failed") == "-ERR Failed");
    }
}

TEST_CASE("Command type conversion", "[pop3][commands]") {
    SECTION("Type to string") {
        REQUIRE(Command::type_to_string(CommandType::USER) == "USER");
        REQUIRE(Command::type_to_string(CommandType::PASS) == "PASS");
        REQUIRE(Command::type_to_string(CommandType::STAT) == "STAT");
        REQUIRE(Command::type_to_string(CommandType::QUIT) == "QUIT");
        REQUIRE(Command::type_to_string(CommandType::UNKNOWN) == "UNKNOWN");
    }

    SECTION("String to type") {
        REQUIRE(Command::string_to_type("USER") == CommandType::USER);
        REQUIRE(Command::string_to_type("PASS") == CommandType::PASS);
        REQUIRE(Command::string_to_type("STAT") == CommandType::STAT);
        REQUIRE(Command::string_to_type("QUIT") == CommandType::QUIT);
        REQUIRE(Command::string_to_type("INVALID") == CommandType::UNKNOWN);
    }
}
