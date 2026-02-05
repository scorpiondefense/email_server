#pragma once

#include <string>
#include <filesystem>
#include <optional>
#include <unordered_map>
#include <vector>
#include <cstdint>

namespace email {

struct ServerConfig {
    std::string bind_address = "0.0.0.0";
    uint16_t port = 0;
    uint16_t tls_port = 0;
    bool enable_starttls = true;
    size_t max_connections = 1000;
    size_t thread_pool_size = 4;
    std::chrono::seconds connection_timeout{300};
    std::chrono::seconds idle_timeout{600};
};

struct TLSConfig {
    std::filesystem::path certificate_file;
    std::filesystem::path private_key_file;
    std::filesystem::path ca_file;
    std::string ciphers = "HIGH:!aNULL:!MD5:!RC4";
    bool verify_client = false;
    int min_protocol_version = 0;  // 0 = TLS 1.2
};

struct DatabaseConfig {
    std::filesystem::path path = "/var/lib/email_server/users.db";
    size_t connection_pool_size = 10;
};

struct StorageConfig {
    std::filesystem::path maildir_root = "/var/mail";
    size_t default_quota_bytes = 104857600;  // 100 MB
    bool create_directories = true;
};

struct LogConfig {
    enum class Level { Trace, Debug, Info, Warning, Error, Fatal };
    Level level = Level::Info;
    std::filesystem::path file;
    bool log_to_console = true;
    bool log_to_file = false;
    size_t max_file_size = 10 * 1024 * 1024;  // 10 MB
    size_t max_files = 5;
};

struct SMTPConfig : ServerConfig {
    std::string hostname = "localhost";
    std::vector<std::string> local_domains;
    size_t max_message_size = 25 * 1024 * 1024;  // 25 MB
    size_t max_recipients = 100;
    bool require_auth = true;
    bool allow_relay = false;
    std::vector<std::string> relay_hosts;

    SMTPConfig() {
        port = 25;
        tls_port = 465;
    }
};

struct POP3Config : ServerConfig {
    bool delete_on_retrieve = false;

    POP3Config() {
        port = 110;
        tls_port = 995;
    }
};

struct IMAPConfig : ServerConfig {
    size_t max_search_results = 1000;
    bool enable_idle = true;

    IMAPConfig() {
        port = 143;
        tls_port = 993;
    }
};

class Config {
public:
    static Config& instance();

    bool load(const std::filesystem::path& config_file);
    bool load_from_string(const std::string& content);

    const TLSConfig& tls() const { return tls_; }
    const DatabaseConfig& database() const { return database_; }
    const StorageConfig& storage() const { return storage_; }
    const LogConfig& log() const { return log_; }
    const SMTPConfig& smtp() const { return smtp_; }
    const POP3Config& pop3() const { return pop3_; }
    const IMAPConfig& imap() const { return imap_; }

    TLSConfig& tls() { return tls_; }
    DatabaseConfig& database() { return database_; }
    StorageConfig& storage() { return storage_; }
    LogConfig& log() { return log_; }
    SMTPConfig& smtp() { return smtp_; }
    POP3Config& pop3() { return pop3_; }
    IMAPConfig& imap() { return imap_; }

    std::optional<std::string> get(const std::string& key) const;
    void set(const std::string& key, const std::string& value);

private:
    Config() = default;
    Config(const Config&) = delete;
    Config& operator=(const Config&) = delete;

    void parse_section(const std::string& section, const std::string& key, const std::string& value);

    TLSConfig tls_;
    DatabaseConfig database_;
    StorageConfig storage_;
    LogConfig log_;
    SMTPConfig smtp_;
    POP3Config pop3_;
    IMAPConfig imap_;

    std::unordered_map<std::string, std::string> custom_values_;
};

}  // namespace email
