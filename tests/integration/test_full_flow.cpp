#include <catch2/catch_test_macros.hpp>
#include "auth/authenticator.hpp"
#include "storage/maildir.hpp"
#include "config.hpp"
#include <filesystem>

using namespace email;

// Helper to create a temporary directory for tests
class TempDirectory {
public:
    TempDirectory() {
        path_ = std::filesystem::temp_directory_path() / "email_server_test" /
                std::to_string(std::chrono::system_clock::now().time_since_epoch().count());
        std::filesystem::create_directories(path_);
    }

    ~TempDirectory() {
        std::filesystem::remove_all(path_);
    }

    const std::filesystem::path& path() const { return path_; }

private:
    std::filesystem::path path_;
};

TEST_CASE("Authenticator integration", "[integration][auth]") {
    TempDirectory temp;
    auto db_path = temp.path() / "users.db";

    Authenticator auth(db_path);
    REQUIRE(auth.initialize());

    SECTION("Create and authenticate user") {
        REQUIRE(auth.create_domain("example.com"));
        REQUIRE(auth.create_user("john", "secret123", "example.com"));

        REQUIRE(auth.authenticate("john@example.com", "secret123"));
        REQUIRE_FALSE(auth.authenticate("john@example.com", "wrongpassword"));
    }

    SECTION("User management") {
        auth.create_domain("test.com");
        auth.create_user("alice", "pass1", "test.com");
        auth.create_user("bob", "pass2", "test.com");

        auto users = auth.list_users("test.com");
        REQUIRE(users.size() == 2);

        auto alice = auth.get_user("alice@test.com");
        REQUIRE(alice.has_value());
        REQUIRE(alice->username == "alice");
        REQUIRE(alice->domain == "test.com");

        REQUIRE(auth.change_password("alice@test.com", "newpass"));
        REQUIRE(auth.authenticate("alice@test.com", "newpass"));
        REQUIRE_FALSE(auth.authenticate("alice@test.com", "pass1"));
    }

    SECTION("Domain management") {
        auth.create_domain("domain1.com");
        auth.create_domain("domain2.com");

        auto domains = auth.list_domains();
        REQUIRE(domains.size() == 2);

        REQUIRE(auth.is_local_domain("domain1.com"));
        REQUIRE_FALSE(auth.is_local_domain("unknown.com"));

        auth.set_domain_active("domain1.com", false);
        // Domain should still exist but be inactive
    }

    SECTION("Email parsing") {
        auto [user1, domain1] = Authenticator::parse_email("user@domain.com");
        REQUIRE(user1 == "user");
        REQUIRE(domain1 == "domain.com");

        auto [user2, domain2] = Authenticator::parse_email("noatsign");
        REQUIRE(user2 == "noatsign");
        REQUIRE(domain2.empty());
    }
}

TEST_CASE("Maildir integration", "[integration][maildir]") {
    TempDirectory temp;

    Maildir maildir(temp.path(), "example.com", "testuser");
    REQUIRE(maildir.initialize());
    REQUIRE(maildir.exists());

    SECTION("Message delivery and retrieval") {
        std::string message = "From: sender@example.com\r\n"
                              "To: testuser@example.com\r\n"
                              "Subject: Test Message\r\n"
                              "\r\n"
                              "This is the body.\r\n";

        std::string id = maildir.deliver(message);
        REQUIRE_FALSE(id.empty());

        auto messages = maildir.list_messages();
        REQUIRE(messages.size() == 1);
        REQUIRE(messages[0].unique_id == id);

        auto content = maildir.get_message_content(id);
        REQUIRE(content.has_value());
        REQUIRE(content->find("Test Message") != std::string::npos);
    }

    SECTION("Mailbox operations") {
        REQUIRE(maildir.create_mailbox("Sent"));
        REQUIRE(maildir.create_mailbox("Drafts"));
        REQUIRE(maildir.create_mailbox("Archive"));

        auto mailboxes = maildir.list_mailboxes();
        REQUIRE(mailboxes.size() >= 4);  // INBOX + created + default folders

        REQUIRE(maildir.rename_mailbox("Archive", "Old"));
        mailboxes = maildir.list_mailboxes();

        bool found_old = false;
        bool found_archive = false;
        for (const auto& mbox : mailboxes) {
            if (mbox == "Old") found_old = true;
            if (mbox == "Archive") found_archive = true;
        }
        REQUIRE(found_old);
        REQUIRE_FALSE(found_archive);

        REQUIRE(maildir.delete_mailbox("Old"));
    }

    SECTION("Message flags") {
        std::string message = "Subject: Flag Test\r\n\r\nBody";
        std::string id = maildir.deliver(message);

        auto msg = maildir.get_message(id);
        REQUIRE(msg.has_value());
        REQUIRE(msg->is_new);

        REQUIRE(maildir.add_flags(id, {'S'}));  // Mark as seen
        msg = maildir.get_message(id);
        REQUIRE(msg->is_seen());
        REQUIRE_FALSE(msg->is_new);

        REQUIRE(maildir.add_flags(id, {'F', 'R'}));  // Flagged and Replied
        msg = maildir.get_message(id);
        REQUIRE(msg->is_flagged());
        REQUIRE(msg->is_answered());

        REQUIRE(maildir.remove_flags(id, {'F'}));
        msg = maildir.get_message(id);
        REQUIRE_FALSE(msg->is_flagged());
        REQUIRE(msg->is_answered());
    }

    SECTION("Message copy and move") {
        maildir.create_mailbox("Folder1");
        maildir.create_mailbox("Folder2");

        std::string message = "Subject: Move Test\r\n\r\nBody";
        std::string id = maildir.deliver(message, "Folder1");

        auto messages = maildir.list_messages("Folder1");
        REQUIRE(messages.size() == 1);

        REQUIRE(maildir.copy_message(id, "Folder1", "Folder2"));
        messages = maildir.list_messages("Folder2");
        REQUIRE(messages.size() == 1);

        // Original should still exist
        messages = maildir.list_messages("Folder1");
        REQUIRE(messages.size() == 1);

        REQUIRE(maildir.move_message(id, "Folder1", "INBOX"));
        messages = maildir.list_messages("Folder1");
        REQUIRE(messages.size() == 0);

        messages = maildir.list_messages("INBOX");
        REQUIRE(messages.size() == 1);
    }

    SECTION("Expunge deleted messages") {
        for (int i = 0; i < 5; ++i) {
            maildir.deliver("Subject: Test " + std::to_string(i) + "\r\n\r\nBody");
        }

        auto messages = maildir.list_messages();
        REQUIRE(messages.size() == 5);

        // Mark some as deleted
        maildir.add_flags(messages[1].unique_id, {'T'});
        maildir.add_flags(messages[3].unique_id, {'T'});

        size_t expunged = maildir.expunge();
        REQUIRE(expunged == 2);

        messages = maildir.list_messages();
        REQUIRE(messages.size() == 3);
    }
}

TEST_CASE("Configuration loading", "[integration][config]") {
    TempDirectory temp;
    auto config_path = temp.path() / "server.conf";

    // Write test configuration
    {
        std::ofstream file(config_path);
        file << "[tls]\n"
             << "certificate = /etc/ssl/certs/mail.crt\n"
             << "private_key = /etc/ssl/private/mail.key\n"
             << "\n"
             << "[database]\n"
             << "path = /var/lib/mail/users.db\n"
             << "\n"
             << "[storage]\n"
             << "maildir_root = /var/mail\n"
             << "default_quota = 104857600\n"
             << "\n"
             << "[smtp]\n"
             << "port = 25\n"
             << "tls_port = 465\n"
             << "hostname = mail.example.com\n"
             << "require_auth = true\n"
             << "local_domains = example.com, example.org\n"
             << "\n"
             << "[pop3]\n"
             << "port = 110\n"
             << "tls_port = 995\n"
             << "\n"
             << "[imap]\n"
             << "port = 143\n"
             << "tls_port = 993\n"
             << "\n"
             << "[log]\n"
             << "level = info\n"
             << "console = true\n";
    }

    Config config;
    REQUIRE(config.load(config_path));

    SECTION("TLS configuration") {
        REQUIRE(config.tls().certificate_file == "/etc/ssl/certs/mail.crt");
        REQUIRE(config.tls().private_key_file == "/etc/ssl/private/mail.key");
    }

    SECTION("Database configuration") {
        REQUIRE(config.database().path == "/var/lib/mail/users.db");
    }

    SECTION("Storage configuration") {
        REQUIRE(config.storage().maildir_root == "/var/mail");
        REQUIRE(config.storage().default_quota_bytes == 104857600);
    }

    SECTION("SMTP configuration") {
        REQUIRE(config.smtp().port == 25);
        REQUIRE(config.smtp().tls_port == 465);
        REQUIRE(config.smtp().hostname == "mail.example.com");
        REQUIRE(config.smtp().require_auth == true);
        REQUIRE(config.smtp().local_domains.size() == 2);
    }

    SECTION("POP3 configuration") {
        REQUIRE(config.pop3().port == 110);
        REQUIRE(config.pop3().tls_port == 995);
    }

    SECTION("IMAP configuration") {
        REQUIRE(config.imap().port == 143);
        REQUIRE(config.imap().tls_port == 993);
    }
}

TEST_CASE("Full email flow simulation", "[integration][flow]") {
    TempDirectory temp;
    auto db_path = temp.path() / "users.db";
    auto mail_path = temp.path() / "mail";

    // Setup authenticator
    Authenticator auth(db_path);
    REQUIRE(auth.initialize());
    auth.create_domain("example.com");
    auth.create_user("alice", "alicepass", "example.com");
    auth.create_user("bob", "bobpass", "example.com");

    // Setup maildirs
    Maildir alice_maildir(mail_path, "example.com", "alice");
    Maildir bob_maildir(mail_path, "example.com", "bob");
    alice_maildir.initialize();
    bob_maildir.initialize();

    SECTION("Simulate email from alice to bob") {
        // 1. Alice composes an email
        std::string email_content =
            "From: alice@example.com\r\n"
            "To: bob@example.com\r\n"
            "Subject: Hello Bob!\r\n"
            "Date: Mon, 01 Jan 2024 12:00:00 +0000\r\n"
            "Message-ID: <unique-id@example.com>\r\n"
            "\r\n"
            "Hi Bob,\r\n"
            "\r\n"
            "This is a test email.\r\n"
            "\r\n"
            "Best,\r\n"
            "Alice\r\n";

        // 2. SMTP delivers to Bob's mailbox
        std::string msg_id = bob_maildir.deliver(email_content);
        REQUIRE_FALSE(msg_id.empty());

        // 3. Also save to Alice's Sent folder
        alice_maildir.deliver(email_content, "Sent");

        // 4. Bob checks his mailbox via POP3 (list messages)
        auto bob_messages = bob_maildir.list_messages();
        REQUIRE(bob_messages.size() == 1);
        REQUIRE(bob_messages[0].is_new);

        // 5. Bob retrieves the message
        auto content = bob_maildir.get_message_content(msg_id);
        REQUIRE(content.has_value());
        REQUIRE(content->find("Hello Bob!") != std::string::npos);

        // 6. Bob marks message as read (IMAP style)
        bob_maildir.mark_as_seen(msg_id);
        auto msg = bob_maildir.get_message(msg_id);
        REQUIRE(msg->is_seen());

        // 7. Bob replies (creates new message in Drafts, then sends)
        std::string reply_content =
            "From: bob@example.com\r\n"
            "To: alice@example.com\r\n"
            "Subject: Re: Hello Bob!\r\n"
            "In-Reply-To: <unique-id@example.com>\r\n"
            "\r\n"
            "Hi Alice,\r\n"
            "\r\n"
            "Thanks for your email!\r\n"
            "\r\n"
            "Bob\r\n";

        // SMTP delivers to Alice
        alice_maildir.deliver(reply_content);

        // 8. Alice checks IMAP for new messages
        auto alice_messages = alice_maildir.list_new_messages();
        REQUIRE(alice_messages.size() == 1);

        // 9. Bob marks original message as replied
        bob_maildir.add_flags(msg_id, {'R'});
        msg = bob_maildir.get_message(msg_id);
        REQUIRE(msg->is_answered());

        // 10. Later, Bob moves message to Archive
        bob_maildir.create_mailbox("Archive");
        bob_maildir.move_message(msg_id, "INBOX", "Archive");

        auto inbox = bob_maildir.list_messages("INBOX");
        auto archive = bob_maildir.list_messages("Archive");
        REQUIRE(inbox.size() == 0);
        REQUIRE(archive.size() == 1);
    }
}
