#include <catch2/catch_test_macros.hpp>
#include "smtp_commands.hpp"

using namespace email::smtp;

TEST_CASE("SMTP command parsing", "[smtp][commands]") {
    SECTION("Parse HELO command") {
        auto cmd = Command::parse("HELO client.example.com");
        REQUIRE(cmd.type == CommandType::HELO);
        REQUIRE(cmd.argument == "client.example.com");
    }

    SECTION("Parse EHLO command") {
        auto cmd = Command::parse("EHLO client.example.com");
        REQUIRE(cmd.type == CommandType::EHLO);
        REQUIRE(cmd.argument == "client.example.com");
    }

    SECTION("Parse MAIL FROM command") {
        auto cmd = Command::parse("MAIL FROM:<sender@example.com>");
        REQUIRE(cmd.type == CommandType::MAIL);
        REQUIRE(cmd.argument == "<sender@example.com>");
    }

    SECTION("Parse RCPT TO command") {
        auto cmd = Command::parse("RCPT TO:<recipient@example.com>");
        REQUIRE(cmd.type == CommandType::RCPT);
        REQUIRE(cmd.argument == "<recipient@example.com>");
    }

    SECTION("Parse DATA command") {
        auto cmd = Command::parse("DATA");
        REQUIRE(cmd.type == CommandType::DATA);
    }

    SECTION("Parse RSET command") {
        auto cmd = Command::parse("RSET");
        REQUIRE(cmd.type == CommandType::RSET);
    }

    SECTION("Parse NOOP command") {
        auto cmd = Command::parse("NOOP");
        REQUIRE(cmd.type == CommandType::NOOP);
    }

    SECTION("Parse QUIT command") {
        auto cmd = Command::parse("QUIT");
        REQUIRE(cmd.type == CommandType::QUIT);
    }

    SECTION("Parse AUTH command") {
        auto cmd = Command::parse("AUTH PLAIN");
        REQUIRE(cmd.type == CommandType::AUTH);
        REQUIRE(cmd.argument == "PLAIN");
    }

    SECTION("Parse STARTTLS command") {
        auto cmd = Command::parse("STARTTLS");
        REQUIRE(cmd.type == CommandType::STARTTLS);
    }

    SECTION("Parse unknown command") {
        auto cmd = Command::parse("INVALID");
        REQUIRE(cmd.type == CommandType::UNKNOWN);
    }

    SECTION("Case insensitive parsing") {
        auto cmd1 = Command::parse("helo client.example.com");
        auto cmd2 = Command::parse("Helo client.example.com");
        REQUIRE(cmd1.type == CommandType::HELO);
        REQUIRE(cmd2.type == CommandType::HELO);
    }
}

TEST_CASE("Email address parsing", "[smtp][email]") {
    SECTION("Simple email address") {
        auto addr = EmailAddress::parse("user@example.com");
        REQUIRE(addr.has_value());
        REQUIRE(addr->local_part == "user");
        REQUIRE(addr->domain == "example.com");
    }

    SECTION("Email in angle brackets") {
        auto addr = EmailAddress::parse("<user@example.com>");
        REQUIRE(addr.has_value());
        REQUIRE(addr->local_part == "user");
        REQUIRE(addr->domain == "example.com");
    }

    SECTION("Email with whitespace") {
        auto addr = EmailAddress::parse("  <user@example.com>  ");
        REQUIRE(addr.has_value());
        REQUIRE(addr->local_part == "user");
        REQUIRE(addr->domain == "example.com");
    }

    SECTION("Null sender") {
        auto addr = EmailAddress::parse("<>");
        REQUIRE(addr.has_value());
        REQUIRE(addr->local_part.empty());
        REQUIRE(addr->domain.empty());
    }

    SECTION("Invalid - no @") {
        auto addr = EmailAddress::parse("userexample.com");
        REQUIRE_FALSE(addr.has_value());
    }

    SECTION("Invalid - empty local part") {
        auto addr = EmailAddress::parse("@example.com");
        REQUIRE_FALSE(addr.has_value());
    }

    SECTION("Invalid - empty domain") {
        auto addr = EmailAddress::parse("user@");
        REQUIRE_FALSE(addr.has_value());
    }
}

TEST_CASE("SMTP reply codes", "[smtp][reply]") {
    SECTION("Make simple reply") {
        auto reply_str = reply::make(250, "OK");
        REQUIRE(reply_str == "250 OK");
    }

    SECTION("Make multi-line reply") {
        std::vector<std::string> lines = {"Hello", "PIPELINING", "SIZE 10240000"};
        auto reply_str = reply::make_multi(250, lines);
        REQUIRE(reply_str.find("250-Hello") != std::string::npos);
        REQUIRE(reply_str.find("250-PIPELINING") != std::string::npos);
        REQUIRE(reply_str.find("250 SIZE") != std::string::npos);
    }

    SECTION("Reply code constants") {
        REQUIRE(reply::SERVICE_READY == 220);
        REQUIRE(reply::OK == 250);
        REQUIRE(reply::START_MAIL_INPUT == 354);
        REQUIRE(reply::SYNTAX_ERROR == 500);
        REQUIRE(reply::AUTH_REQUIRED == 530);
    }
}

TEST_CASE("Command type conversion", "[smtp][commands]") {
    SECTION("Type to string") {
        REQUIRE(Command::type_to_string(CommandType::HELO) == "HELO");
        REQUIRE(Command::type_to_string(CommandType::EHLO) == "EHLO");
        REQUIRE(Command::type_to_string(CommandType::MAIL) == "MAIL");
        REQUIRE(Command::type_to_string(CommandType::RCPT) == "RCPT");
        REQUIRE(Command::type_to_string(CommandType::DATA) == "DATA");
        REQUIRE(Command::type_to_string(CommandType::QUIT) == "QUIT");
        REQUIRE(Command::type_to_string(CommandType::UNKNOWN) == "UNKNOWN");
    }

    SECTION("String to type") {
        REQUIRE(Command::string_to_type("HELO") == CommandType::HELO);
        REQUIRE(Command::string_to_type("EHLO") == CommandType::EHLO);
        REQUIRE(Command::string_to_type("MAIL FROM:") == CommandType::MAIL);
        REQUIRE(Command::string_to_type("RCPT TO:") == CommandType::RCPT);
        REQUIRE(Command::string_to_type("DATA") == CommandType::DATA);
        REQUIRE(Command::string_to_type("QUIT") == CommandType::QUIT);
        REQUIRE(Command::string_to_type("INVALID") == CommandType::UNKNOWN);
    }
}
