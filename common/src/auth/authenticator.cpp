#include "auth/authenticator.hpp"
#include "logger.hpp"
#include <sqlite3.h>
#include <random>
#include <sstream>
#include <iomanip>
#include <openssl/sha.h>
#include <openssl/evp.h>
#include <openssl/hmac.h>
#include <cstring>

namespace email {

Authenticator::Authenticator(const std::filesystem::path& db_path)
    : db_path_(db_path) {
}

Authenticator::~Authenticator() {
    if (db_) {
        sqlite3_close(db_);
    }
}

bool Authenticator::initialize() {
    std::lock_guard<std::mutex> lock(mutex_);

    // Create parent directories if they don't exist
    if (auto parent = db_path_.parent_path(); !parent.empty()) {
        std::filesystem::create_directories(parent);
    }

    int rc = sqlite3_open(db_path_.string().c_str(), &db_);
    if (rc != SQLITE_OK) {
        last_error_ = std::string("Cannot open database: ") + sqlite3_errmsg(db_);
        LOG_ERROR(last_error_);
        return false;
    }

    // Enable WAL mode for better concurrency
    execute_sql("PRAGMA journal_mode=WAL;");
    execute_sql("PRAGMA foreign_keys=ON;");

    return create_tables();
}

bool Authenticator::create_tables() {
    const char* domains_table = R"(
        CREATE TABLE IF NOT EXISTS domains (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            domain TEXT UNIQUE NOT NULL,
            active INTEGER DEFAULT 1,
            created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP
        );
    )";

    const char* users_table = R"(
        CREATE TABLE IF NOT EXISTS users (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            username TEXT NOT NULL,
            domain TEXT NOT NULL,
            password_hash TEXT NOT NULL,
            quota_bytes INTEGER DEFAULT 104857600,
            used_bytes INTEGER DEFAULT 0,
            active INTEGER DEFAULT 1,
            created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
            UNIQUE(username, domain),
            FOREIGN KEY(domain) REFERENCES domains(domain) ON DELETE CASCADE
        );
    )";

    const char* index_username = R"(
        CREATE INDEX IF NOT EXISTS idx_users_username ON users(username);
    )";

    const char* index_domain = R"(
        CREATE INDEX IF NOT EXISTS idx_users_domain ON users(domain);
    )";

    if (!execute_sql(domains_table)) return false;
    if (!execute_sql(users_table)) return false;
    if (!execute_sql(index_username)) return false;
    if (!execute_sql(index_domain)) return false;

    return true;
}

bool Authenticator::execute_sql(const std::string& sql) {
    char* err_msg = nullptr;
    int rc = sqlite3_exec(db_, sql.c_str(), nullptr, nullptr, &err_msg);
    if (rc != SQLITE_OK) {
        last_error_ = std::string("SQL error: ") + (err_msg ? err_msg : "unknown");
        sqlite3_free(err_msg);
        LOG_ERROR(last_error_);
        return false;
    }
    return true;
}

bool Authenticator::authenticate(const std::string& username, const std::string& password) {
    return authenticate_plain(username, password);
}

bool Authenticator::authenticate_plain(const std::string& username, const std::string& password) {
    std::lock_guard<std::mutex> lock(mutex_);

    auto [user, domain] = parse_email(username);

    const char* sql = "SELECT password_hash, active FROM users WHERE username = ? AND domain = ?;";
    sqlite3_stmt* stmt;

    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        last_error_ = sqlite3_errmsg(db_);
        return false;
    }

    sqlite3_bind_text(stmt, 1, user.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 2, domain.c_str(), -1, SQLITE_TRANSIENT);

    bool result = false;
    if (sqlite3_step(stmt) == SQLITE_ROW) {
        std::string stored_hash = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0));
        int active = sqlite3_column_int(stmt, 1);

        if (active && verify_password(password, stored_hash)) {
            result = true;
        }
    }

    sqlite3_finalize(stmt);
    return result;
}

bool Authenticator::authenticate_login(const std::string& username, const std::string& password) {
    return authenticate_plain(username, password);
}

bool Authenticator::authenticate_cram_md5(const std::string& username,
                                          const std::string& challenge,
                                          const std::string& response) {
    std::lock_guard<std::mutex> lock(mutex_);

    auto [user, domain] = parse_email(username);

    // Get the stored password hash (we need the actual password for CRAM-MD5)
    // Note: CRAM-MD5 requires storing passwords in retrievable form, which is less secure
    // For this implementation, we store a secondary plain password for CRAM-MD5
    // In production, consider not supporting CRAM-MD5

    const char* sql = "SELECT password_hash FROM users WHERE username = ? AND domain = ? AND active = 1;";
    sqlite3_stmt* stmt;

    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        return false;
    }

    sqlite3_bind_text(stmt, 1, user.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 2, domain.c_str(), -1, SQLITE_TRANSIENT);

    bool result = false;
    if (sqlite3_step(stmt) == SQLITE_ROW) {
        // For CRAM-MD5, compute HMAC-MD5(challenge, password)
        // This is a simplified implementation - in practice you'd need
        // to store passwords differently to support CRAM-MD5
        (void)challenge;
        (void)response;
        // result = (computed_response == response);
    }

    sqlite3_finalize(stmt);
    return result;
}

bool Authenticator::create_user(const std::string& username, const std::string& password,
                                const std::string& domain, int64_t quota_bytes) {
    std::lock_guard<std::mutex> lock(mutex_);

    // Ensure domain exists
    if (!is_local_domain(domain)) {
        // Create domain if it doesn't exist
        const char* domain_sql = "INSERT OR IGNORE INTO domains (domain) VALUES (?);";
        sqlite3_stmt* stmt;
        if (sqlite3_prepare_v2(db_, domain_sql, -1, &stmt, nullptr) == SQLITE_OK) {
            sqlite3_bind_text(stmt, 1, domain.c_str(), -1, SQLITE_TRANSIENT);
            sqlite3_step(stmt);
            sqlite3_finalize(stmt);
        }
    }

    std::string password_hash = hash_password(password);

    const char* sql = "INSERT INTO users (username, domain, password_hash, quota_bytes) VALUES (?, ?, ?, ?);";
    sqlite3_stmt* stmt;

    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        last_error_ = sqlite3_errmsg(db_);
        return false;
    }

    sqlite3_bind_text(stmt, 1, username.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 2, domain.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 3, password_hash.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_int64(stmt, 4, quota_bytes);

    bool result = (sqlite3_step(stmt) == SQLITE_DONE);
    if (!result) {
        last_error_ = sqlite3_errmsg(db_);
    }

    sqlite3_finalize(stmt);
    return result;
}

bool Authenticator::delete_user(const std::string& username) {
    std::lock_guard<std::mutex> lock(mutex_);

    auto [user, domain] = parse_email(username);

    const char* sql = "DELETE FROM users WHERE username = ? AND domain = ?;";
    sqlite3_stmt* stmt;

    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        last_error_ = sqlite3_errmsg(db_);
        return false;
    }

    sqlite3_bind_text(stmt, 1, user.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 2, domain.c_str(), -1, SQLITE_TRANSIENT);

    bool result = (sqlite3_step(stmt) == SQLITE_DONE);
    sqlite3_finalize(stmt);
    return result && sqlite3_changes(db_) > 0;
}

bool Authenticator::change_password(const std::string& username, const std::string& new_password) {
    std::lock_guard<std::mutex> lock(mutex_);

    auto [user, domain] = parse_email(username);
    std::string password_hash = hash_password(new_password);

    const char* sql = "UPDATE users SET password_hash = ? WHERE username = ? AND domain = ?;";
    sqlite3_stmt* stmt;

    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        last_error_ = sqlite3_errmsg(db_);
        return false;
    }

    sqlite3_bind_text(stmt, 1, password_hash.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 2, user.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 3, domain.c_str(), -1, SQLITE_TRANSIENT);

    bool result = (sqlite3_step(stmt) == SQLITE_DONE);
    sqlite3_finalize(stmt);
    return result && sqlite3_changes(db_) > 0;
}

bool Authenticator::set_user_active(const std::string& username, bool active) {
    std::lock_guard<std::mutex> lock(mutex_);

    auto [user, domain] = parse_email(username);

    const char* sql = "UPDATE users SET active = ? WHERE username = ? AND domain = ?;";
    sqlite3_stmt* stmt;

    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        return false;
    }

    sqlite3_bind_int(stmt, 1, active ? 1 : 0);
    sqlite3_bind_text(stmt, 2, user.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 3, domain.c_str(), -1, SQLITE_TRANSIENT);

    bool result = (sqlite3_step(stmt) == SQLITE_DONE);
    sqlite3_finalize(stmt);
    return result;
}

bool Authenticator::update_quota(const std::string& username, int64_t quota_bytes) {
    std::lock_guard<std::mutex> lock(mutex_);

    auto [user, domain] = parse_email(username);

    const char* sql = "UPDATE users SET quota_bytes = ? WHERE username = ? AND domain = ?;";
    sqlite3_stmt* stmt;

    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        return false;
    }

    sqlite3_bind_int64(stmt, 1, quota_bytes);
    sqlite3_bind_text(stmt, 2, user.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 3, domain.c_str(), -1, SQLITE_TRANSIENT);

    bool result = (sqlite3_step(stmt) == SQLITE_DONE);
    sqlite3_finalize(stmt);
    return result;
}

bool Authenticator::update_used_space(const std::string& username, int64_t used_bytes) {
    std::lock_guard<std::mutex> lock(mutex_);

    auto [user, domain] = parse_email(username);

    const char* sql = "UPDATE users SET used_bytes = ? WHERE username = ? AND domain = ?;";
    sqlite3_stmt* stmt;

    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        return false;
    }

    sqlite3_bind_int64(stmt, 1, used_bytes);
    sqlite3_bind_text(stmt, 2, user.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 3, domain.c_str(), -1, SQLITE_TRANSIENT);

    bool result = (sqlite3_step(stmt) == SQLITE_DONE);
    sqlite3_finalize(stmt);
    return result;
}

std::optional<User> Authenticator::get_user(const std::string& username) {
    std::lock_guard<std::mutex> lock(mutex_);

    auto [user, domain] = parse_email(username);

    const char* sql = "SELECT id, username, domain, quota_bytes, used_bytes, active, created_at "
                      "FROM users WHERE username = ? AND domain = ?;";
    sqlite3_stmt* stmt;

    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        return std::nullopt;
    }

    sqlite3_bind_text(stmt, 1, user.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 2, domain.c_str(), -1, SQLITE_TRANSIENT);

    std::optional<User> result;
    if (sqlite3_step(stmt) == SQLITE_ROW) {
        User u;
        u.id = sqlite3_column_int64(stmt, 0);
        u.username = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));
        u.domain = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 2));
        u.quota_bytes = sqlite3_column_int64(stmt, 3);
        u.used_bytes = sqlite3_column_int64(stmt, 4);
        u.active = sqlite3_column_int(stmt, 5) != 0;
        if (sqlite3_column_text(stmt, 6)) {
            u.created_at = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 6));
        }
        result = u;
    }

    sqlite3_finalize(stmt);
    return result;
}

std::vector<User> Authenticator::list_users(const std::string& domain) {
    std::lock_guard<std::mutex> lock(mutex_);

    std::string sql = "SELECT id, username, domain, quota_bytes, used_bytes, active, created_at FROM users";
    if (!domain.empty()) {
        sql += " WHERE domain = ?";
    }
    sql += " ORDER BY username;";

    sqlite3_stmt* stmt;
    if (sqlite3_prepare_v2(db_, sql.c_str(), -1, &stmt, nullptr) != SQLITE_OK) {
        return {};
    }

    if (!domain.empty()) {
        sqlite3_bind_text(stmt, 1, domain.c_str(), -1, SQLITE_TRANSIENT);
    }

    std::vector<User> users;
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        User u;
        u.id = sqlite3_column_int64(stmt, 0);
        u.username = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));
        u.domain = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 2));
        u.quota_bytes = sqlite3_column_int64(stmt, 3);
        u.used_bytes = sqlite3_column_int64(stmt, 4);
        u.active = sqlite3_column_int(stmt, 5) != 0;
        if (sqlite3_column_text(stmt, 6)) {
            u.created_at = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 6));
        }
        users.push_back(u);
    }

    sqlite3_finalize(stmt);
    return users;
}

bool Authenticator::create_domain(const std::string& domain) {
    std::lock_guard<std::mutex> lock(mutex_);

    const char* sql = "INSERT INTO domains (domain) VALUES (?);";
    sqlite3_stmt* stmt;

    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        last_error_ = sqlite3_errmsg(db_);
        return false;
    }

    sqlite3_bind_text(stmt, 1, domain.c_str(), -1, SQLITE_TRANSIENT);

    bool result = (sqlite3_step(stmt) == SQLITE_DONE);
    if (!result) {
        last_error_ = sqlite3_errmsg(db_);
    }

    sqlite3_finalize(stmt);
    return result;
}

bool Authenticator::delete_domain(const std::string& domain) {
    std::lock_guard<std::mutex> lock(mutex_);

    const char* sql = "DELETE FROM domains WHERE domain = ?;";
    sqlite3_stmt* stmt;

    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        return false;
    }

    sqlite3_bind_text(stmt, 1, domain.c_str(), -1, SQLITE_TRANSIENT);

    bool result = (sqlite3_step(stmt) == SQLITE_DONE);
    sqlite3_finalize(stmt);
    return result && sqlite3_changes(db_) > 0;
}

bool Authenticator::set_domain_active(const std::string& domain, bool active) {
    std::lock_guard<std::mutex> lock(mutex_);

    const char* sql = "UPDATE domains SET active = ? WHERE domain = ?;";
    sqlite3_stmt* stmt;

    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        return false;
    }

    sqlite3_bind_int(stmt, 1, active ? 1 : 0);
    sqlite3_bind_text(stmt, 2, domain.c_str(), -1, SQLITE_TRANSIENT);

    bool result = (sqlite3_step(stmt) == SQLITE_DONE);
    sqlite3_finalize(stmt);
    return result;
}

bool Authenticator::is_local_domain(const std::string& domain) {
    // Note: Don't lock here since it's called from locked context
    const char* sql = "SELECT 1 FROM domains WHERE domain = ? AND active = 1;";
    sqlite3_stmt* stmt;

    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        return false;
    }

    sqlite3_bind_text(stmt, 1, domain.c_str(), -1, SQLITE_TRANSIENT);

    bool result = (sqlite3_step(stmt) == SQLITE_ROW);
    sqlite3_finalize(stmt);
    return result;
}

std::optional<Domain> Authenticator::get_domain(const std::string& domain) {
    std::lock_guard<std::mutex> lock(mutex_);

    const char* sql = "SELECT id, domain, active FROM domains WHERE domain = ?;";
    sqlite3_stmt* stmt;

    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        return std::nullopt;
    }

    sqlite3_bind_text(stmt, 1, domain.c_str(), -1, SQLITE_TRANSIENT);

    std::optional<Domain> result;
    if (sqlite3_step(stmt) == SQLITE_ROW) {
        Domain d;
        d.id = sqlite3_column_int64(stmt, 0);
        d.domain = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));
        d.active = sqlite3_column_int(stmt, 2) != 0;
        result = d;
    }

    sqlite3_finalize(stmt);
    return result;
}

std::vector<Domain> Authenticator::list_domains() {
    std::lock_guard<std::mutex> lock(mutex_);

    const char* sql = "SELECT id, domain, active FROM domains ORDER BY domain;";
    sqlite3_stmt* stmt;

    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        return {};
    }

    std::vector<Domain> domains;
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        Domain d;
        d.id = sqlite3_column_int64(stmt, 0);
        d.domain = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));
        d.active = sqlite3_column_int(stmt, 2) != 0;
        domains.push_back(d);
    }

    sqlite3_finalize(stmt);
    return domains;
}

std::pair<std::string, std::string> Authenticator::parse_email(const std::string& email) {
    auto at_pos = email.find('@');
    if (at_pos == std::string::npos) {
        return {email, ""};
    }
    return {email.substr(0, at_pos), email.substr(at_pos + 1)};
}

std::string Authenticator::generate_salt(size_t length) {
    static const char charset[] =
        "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz./";

    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> dis(0, sizeof(charset) - 2);

    std::string salt;
    salt.reserve(length);
    for (size_t i = 0; i < length; ++i) {
        salt += charset[dis(gen)];
    }
    return salt;
}

std::string Authenticator::hash_password(const std::string& password) {
    // Using PBKDF2-SHA256 for password hashing
    std::string salt = generate_salt(16);
    const int iterations = 100000;
    const int key_length = 32;

    unsigned char derived_key[32];

    PKCS5_PBKDF2_HMAC(
        password.c_str(), static_cast<int>(password.length()),
        reinterpret_cast<const unsigned char*>(salt.c_str()), static_cast<int>(salt.length()),
        iterations,
        EVP_sha256(),
        key_length, derived_key
    );

    // Format: $pbkdf2-sha256$iterations$salt$hash
    std::ostringstream oss;
    oss << "$pbkdf2-sha256$" << iterations << "$" << salt << "$";

    for (int i = 0; i < key_length; ++i) {
        oss << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(derived_key[i]);
    }

    return oss.str();
}

bool Authenticator::verify_password(const std::string& password, const std::string& hash) {
    // Parse the hash format: $pbkdf2-sha256$iterations$salt$hash
    if (hash.substr(0, 14) != "$pbkdf2-sha256") {
        return false;
    }

    size_t pos1 = 14;  // After "$pbkdf2-sha256"
    if (hash[pos1] != '$') return false;
    pos1++;

    size_t pos2 = hash.find('$', pos1);
    if (pos2 == std::string::npos) return false;

    int iterations = std::stoi(hash.substr(pos1, pos2 - pos1));

    size_t pos3 = hash.find('$', pos2 + 1);
    if (pos3 == std::string::npos) return false;

    std::string salt = hash.substr(pos2 + 1, pos3 - pos2 - 1);
    std::string stored_hash = hash.substr(pos3 + 1);

    const int key_length = 32;
    unsigned char derived_key[32];

    PKCS5_PBKDF2_HMAC(
        password.c_str(), static_cast<int>(password.length()),
        reinterpret_cast<const unsigned char*>(salt.c_str()), static_cast<int>(salt.length()),
        iterations,
        EVP_sha256(),
        key_length, derived_key
    );

    std::ostringstream oss;
    for (int i = 0; i < key_length; ++i) {
        oss << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(derived_key[i]);
    }

    return oss.str() == stored_hash;
}

}  // namespace email
