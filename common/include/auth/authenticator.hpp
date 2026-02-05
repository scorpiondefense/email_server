#pragma once

#include <string>
#include <optional>
#include <memory>
#include <filesystem>
#include <vector>
#include <mutex>
#include <cstdint>

struct sqlite3;

namespace email {

struct User {
    int64_t id = 0;
    std::string username;
    std::string domain;
    int64_t quota_bytes = 104857600;  // 100 MB default
    int64_t used_bytes = 0;
    bool active = true;
    std::string created_at;
};

struct Domain {
    int64_t id = 0;
    std::string domain;
    bool active = true;
};

class Authenticator {
public:
    explicit Authenticator(const std::filesystem::path& db_path);
    ~Authenticator();

    Authenticator(const Authenticator&) = delete;
    Authenticator& operator=(const Authenticator&) = delete;

    bool initialize();

    // Authentication
    bool authenticate(const std::string& username, const std::string& password);
    bool authenticate_plain(const std::string& username, const std::string& password);
    bool authenticate_login(const std::string& username, const std::string& password);
    bool authenticate_cram_md5(const std::string& username,
                               const std::string& challenge,
                               const std::string& response);

    // User management
    bool create_user(const std::string& username, const std::string& password,
                     const std::string& domain, int64_t quota_bytes = 104857600);
    bool delete_user(const std::string& username);
    bool change_password(const std::string& username, const std::string& new_password);
    bool set_user_active(const std::string& username, bool active);
    bool update_quota(const std::string& username, int64_t quota_bytes);
    bool update_used_space(const std::string& username, int64_t used_bytes);

    std::optional<User> get_user(const std::string& username);
    std::vector<User> list_users(const std::string& domain = "");

    // Domain management
    bool create_domain(const std::string& domain);
    bool delete_domain(const std::string& domain);
    bool set_domain_active(const std::string& domain, bool active);
    bool is_local_domain(const std::string& domain);

    std::optional<Domain> get_domain(const std::string& domain);
    std::vector<Domain> list_domains();

    // Email address parsing
    static std::pair<std::string, std::string> parse_email(const std::string& email);

    std::string last_error() const { return last_error_; }

private:
    bool execute_sql(const std::string& sql);
    bool create_tables();

    // Password hashing (using bcrypt-like approach with SHA-256 + salt)
    std::string hash_password(const std::string& password);
    bool verify_password(const std::string& password, const std::string& hash);
    std::string generate_salt(size_t length = 16);

    std::filesystem::path db_path_;
    sqlite3* db_ = nullptr;
    std::string last_error_;
    std::mutex mutex_;
};

}  // namespace email
