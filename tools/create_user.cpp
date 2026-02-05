#include "auth/authenticator.hpp"
#include "storage/maildir.hpp"
#include "config.hpp"
#include <iostream>
#include <string>
#include <cstring>
#include <termios.h>
#include <unistd.h>

void print_usage(const char* program) {
    std::cout << "Email Server User Management Tool\n\n"
              << "Usage: " << program << " <command> [options]\n\n"
              << "Commands:\n"
              << "  add <email> [--quota <bytes>]    Add a new user\n"
              << "  delete <email>                   Delete a user\n"
              << "  passwd <email>                   Change user password\n"
              << "  list [domain]                    List users\n"
              << "  domain add <domain>              Add a domain\n"
              << "  domain delete <domain>           Delete a domain\n"
              << "  domain list                      List domains\n"
              << "  info <email>                     Show user info\n\n"
              << "Options:\n"
              << "  -c, --config <file>    Configuration file (default: /etc/email_server/server.conf)\n"
              << "  -d, --database <file>  Database file (overrides config)\n"
              << "  -m, --maildir <path>   Maildir root (overrides config)\n"
              << "  -q, --quota <bytes>    Storage quota in bytes (default: 104857600)\n"
              << "  -h, --help             Show this help\n";
}

std::string read_password(const std::string& prompt) {
    std::cout << prompt;
    std::cout.flush();

    // Disable echo
    termios old_term, new_term;
    tcgetattr(STDIN_FILENO, &old_term);
    new_term = old_term;
    new_term.c_lflag &= ~ECHO;
    tcsetattr(STDIN_FILENO, TCSANOW, &new_term);

    std::string password;
    std::getline(std::cin, password);

    // Restore echo
    tcsetattr(STDIN_FILENO, TCSANOW, &old_term);
    std::cout << "\n";

    return password;
}

int main(int argc, char* argv[]) {
    if (argc < 2) {
        print_usage(argv[0]);
        return 1;
    }

    std::string config_file = "/etc/email_server/server.conf";
    std::string db_file;
    std::string maildir_root;
    int64_t quota = 104857600;

    // Parse global options
    int cmd_start = 1;
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "-h" || arg == "--help") {
            print_usage(argv[0]);
            return 0;
        } else if ((arg == "-c" || arg == "--config") && i + 1 < argc) {
            config_file = argv[++i];
            cmd_start = i + 1;
        } else if ((arg == "-d" || arg == "--database") && i + 1 < argc) {
            db_file = argv[++i];
            cmd_start = i + 1;
        } else if ((arg == "-m" || arg == "--maildir") && i + 1 < argc) {
            maildir_root = argv[++i];
            cmd_start = i + 1;
        } else if ((arg == "-q" || arg == "--quota") && i + 1 < argc) {
            quota = std::stoll(argv[++i]);
            cmd_start = i + 1;
        } else if (arg[0] != '-') {
            cmd_start = i;
            break;
        }
    }

    // Load configuration
    auto& config = email::Config::instance();
    config.load(config_file);

    if (db_file.empty()) {
        db_file = config.database().path;
    }
    if (maildir_root.empty()) {
        maildir_root = config.storage().maildir_root;
    }

    // Initialize authenticator
    email::Authenticator auth(db_file);
    if (!auth.initialize()) {
        std::cerr << "Failed to initialize database: " << auth.last_error() << "\n";
        return 1;
    }

    if (cmd_start >= argc) {
        print_usage(argv[0]);
        return 1;
    }

    std::string command = argv[cmd_start];

    if (command == "add") {
        if (cmd_start + 1 >= argc) {
            std::cerr << "Usage: add <email>\n";
            return 1;
        }

        std::string email = argv[cmd_start + 1];

        // Parse additional options
        for (int i = cmd_start + 2; i < argc; ++i) {
            if ((std::string(argv[i]) == "-q" || std::string(argv[i]) == "--quota") && i + 1 < argc) {
                quota = std::stoll(argv[++i]);
            }
        }

        auto [username, domain] = email::Authenticator::parse_email(email);
        if (domain.empty()) {
            std::cerr << "Invalid email format. Use: user@domain.com\n";
            return 1;
        }

        std::string password = read_password("Enter password: ");
        std::string password_confirm = read_password("Confirm password: ");

        if (password != password_confirm) {
            std::cerr << "Passwords do not match.\n";
            return 1;
        }

        if (password.empty()) {
            std::cerr << "Password cannot be empty.\n";
            return 1;
        }

        // Ensure domain exists
        if (!auth.is_local_domain(domain)) {
            if (!auth.create_domain(domain)) {
                std::cerr << "Failed to create domain: " << auth.last_error() << "\n";
                return 1;
            }
            std::cout << "Created domain: " << domain << "\n";
        }

        if (!auth.create_user(username, password, domain, quota)) {
            std::cerr << "Failed to create user: " << auth.last_error() << "\n";
            return 1;
        }

        // Initialize maildir
        email::Maildir maildir(maildir_root, domain, username);
        if (!maildir.initialize()) {
            std::cerr << "Warning: Failed to initialize maildir: " << maildir.last_error() << "\n";
        }

        std::cout << "User created: " << email << "\n";
        std::cout << "Quota: " << quota << " bytes\n";
        std::cout << "Maildir: " << maildir.path() << "\n";

    } else if (command == "delete") {
        if (cmd_start + 1 >= argc) {
            std::cerr << "Usage: delete <email>\n";
            return 1;
        }

        std::string email = argv[cmd_start + 1];

        std::cout << "Are you sure you want to delete " << email << "? (yes/no): ";
        std::string confirm;
        std::getline(std::cin, confirm);

        if (confirm != "yes") {
            std::cout << "Aborted.\n";
            return 0;
        }

        if (!auth.delete_user(email)) {
            std::cerr << "Failed to delete user: " << auth.last_error() << "\n";
            return 1;
        }

        std::cout << "User deleted: " << email << "\n";

    } else if (command == "passwd") {
        if (cmd_start + 1 >= argc) {
            std::cerr << "Usage: passwd <email>\n";
            return 1;
        }

        std::string email = argv[cmd_start + 1];

        auto user = auth.get_user(email);
        if (!user) {
            std::cerr << "User not found: " << email << "\n";
            return 1;
        }

        std::string password = read_password("Enter new password: ");
        std::string password_confirm = read_password("Confirm new password: ");

        if (password != password_confirm) {
            std::cerr << "Passwords do not match.\n";
            return 1;
        }

        if (!auth.change_password(email, password)) {
            std::cerr << "Failed to change password: " << auth.last_error() << "\n";
            return 1;
        }

        std::cout << "Password changed for: " << email << "\n";

    } else if (command == "list") {
        std::string domain;
        if (cmd_start + 1 < argc) {
            domain = argv[cmd_start + 1];
        }

        auto users = auth.list_users(domain);

        if (users.empty()) {
            std::cout << "No users found.\n";
        } else {
            std::cout << "Users:\n";
            for (const auto& user : users) {
                std::cout << "  " << user.username << "@" << user.domain;
                if (!user.active) {
                    std::cout << " (inactive)";
                }
                std::cout << "\n";
            }
        }

    } else if (command == "domain") {
        if (cmd_start + 1 >= argc) {
            std::cerr << "Usage: domain <add|delete|list> [domain]\n";
            return 1;
        }

        std::string subcommand = argv[cmd_start + 1];

        if (subcommand == "add") {
            if (cmd_start + 2 >= argc) {
                std::cerr << "Usage: domain add <domain>\n";
                return 1;
            }

            std::string domain = argv[cmd_start + 2];
            if (!auth.create_domain(domain)) {
                std::cerr << "Failed to create domain: " << auth.last_error() << "\n";
                return 1;
            }
            std::cout << "Domain created: " << domain << "\n";

        } else if (subcommand == "delete") {
            if (cmd_start + 2 >= argc) {
                std::cerr << "Usage: domain delete <domain>\n";
                return 1;
            }

            std::string domain = argv[cmd_start + 2];
            if (!auth.delete_domain(domain)) {
                std::cerr << "Failed to delete domain: " << auth.last_error() << "\n";
                return 1;
            }
            std::cout << "Domain deleted: " << domain << "\n";

        } else if (subcommand == "list") {
            auto domains = auth.list_domains();

            if (domains.empty()) {
                std::cout << "No domains found.\n";
            } else {
                std::cout << "Domains:\n";
                for (const auto& domain : domains) {
                    std::cout << "  " << domain.domain;
                    if (!domain.active) {
                        std::cout << " (inactive)";
                    }
                    std::cout << "\n";
                }
            }
        } else {
            std::cerr << "Unknown domain subcommand: " << subcommand << "\n";
            return 1;
        }

    } else if (command == "info") {
        if (cmd_start + 1 >= argc) {
            std::cerr << "Usage: info <email>\n";
            return 1;
        }

        std::string email = argv[cmd_start + 1];
        auto user = auth.get_user(email);

        if (!user) {
            std::cerr << "User not found: " << email << "\n";
            return 1;
        }

        std::cout << "User: " << user->username << "@" << user->domain << "\n";
        std::cout << "Active: " << (user->active ? "yes" : "no") << "\n";
        std::cout << "Quota: " << user->quota_bytes << " bytes\n";
        std::cout << "Used: " << user->used_bytes << " bytes\n";
        std::cout << "Created: " << user->created_at << "\n";

        // Show maildir info
        email::Maildir maildir(maildir_root, user->domain, user->username);
        if (maildir.exists()) {
            std::cout << "Maildir: " << maildir.path() << "\n";
            std::cout << "Messages: " << maildir.get_message_count() << "\n";
            std::cout << "Total size: " << maildir.get_total_size() << " bytes\n";
        }

    } else {
        std::cerr << "Unknown command: " << command << "\n";
        print_usage(argv[0]);
        return 1;
    }

    return 0;
}
