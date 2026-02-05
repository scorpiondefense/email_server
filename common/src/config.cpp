#include "config.hpp"
#include <fstream>
#include <sstream>
#include <algorithm>
#include <cctype>

namespace email {

Config& Config::instance() {
    static Config instance;
    return instance;
}

bool Config::load(const std::filesystem::path& config_file) {
    std::ifstream file(config_file);
    if (!file.is_open()) {
        return false;
    }

    std::stringstream buffer;
    buffer << file.rdbuf();
    return load_from_string(buffer.str());
}

bool Config::load_from_string(const std::string& content) {
    std::istringstream stream(content);
    std::string line;
    std::string current_section;

    while (std::getline(stream, line)) {
        // Trim whitespace
        auto start = line.find_first_not_of(" \t");
        if (start == std::string::npos) continue;
        auto end = line.find_last_not_of(" \t\r\n");
        line = line.substr(start, end - start + 1);

        // Skip empty lines and comments
        if (line.empty() || line[0] == '#' || line[0] == ';') continue;

        // Section header
        if (line[0] == '[' && line.back() == ']') {
            current_section = line.substr(1, line.length() - 2);
            std::transform(current_section.begin(), current_section.end(),
                          current_section.begin(), ::tolower);
            continue;
        }

        // Key-value pair
        auto eq_pos = line.find('=');
        if (eq_pos == std::string::npos) continue;

        std::string key = line.substr(0, eq_pos);
        std::string value = line.substr(eq_pos + 1);

        // Trim key and value
        auto trim = [](std::string& s) {
            auto start = s.find_first_not_of(" \t");
            auto end = s.find_last_not_of(" \t");
            if (start != std::string::npos && end != std::string::npos) {
                s = s.substr(start, end - start + 1);
            }
        };
        trim(key);
        trim(value);

        // Remove quotes from value
        if (value.length() >= 2 &&
            ((value.front() == '"' && value.back() == '"') ||
             (value.front() == '\'' && value.back() == '\''))) {
            value = value.substr(1, value.length() - 2);
        }

        std::transform(key.begin(), key.end(), key.begin(), ::tolower);
        parse_section(current_section, key, value);
    }

    return true;
}

void Config::parse_section(const std::string& section, const std::string& key,
                           const std::string& value) {
    auto to_bool = [](const std::string& v) {
        return v == "true" || v == "yes" || v == "1" || v == "on";
    };

    auto to_int = [](const std::string& v) -> int64_t {
        try {
            return std::stoll(v);
        } catch (...) {
            return 0;
        }
    };

    if (section == "tls" || section == "ssl") {
        if (key == "certificate" || key == "cert_file") {
            tls_.certificate_file = value;
        } else if (key == "private_key" || key == "key_file") {
            tls_.private_key_file = value;
        } else if (key == "ca_file") {
            tls_.ca_file = value;
        } else if (key == "ciphers") {
            tls_.ciphers = value;
        } else if (key == "verify_client") {
            tls_.verify_client = to_bool(value);
        }
    } else if (section == "database") {
        if (key == "path") {
            database_.path = value;
        } else if (key == "pool_size") {
            database_.connection_pool_size = static_cast<size_t>(to_int(value));
        }
    } else if (section == "storage") {
        if (key == "maildir_root" || key == "root") {
            storage_.maildir_root = value;
        } else if (key == "default_quota") {
            storage_.default_quota_bytes = static_cast<size_t>(to_int(value));
        } else if (key == "create_directories") {
            storage_.create_directories = to_bool(value);
        }
    } else if (section == "log" || section == "logging") {
        if (key == "level") {
            std::string level = value;
            std::transform(level.begin(), level.end(), level.begin(), ::tolower);
            if (level == "trace") log_.level = LogConfig::Level::Trace;
            else if (level == "debug") log_.level = LogConfig::Level::Debug;
            else if (level == "info") log_.level = LogConfig::Level::Info;
            else if (level == "warning" || level == "warn") log_.level = LogConfig::Level::Warning;
            else if (level == "error") log_.level = LogConfig::Level::Error;
            else if (level == "fatal") log_.level = LogConfig::Level::Fatal;
        } else if (key == "file") {
            log_.file = value;
            log_.log_to_file = !value.empty();
        } else if (key == "console") {
            log_.log_to_console = to_bool(value);
        } else if (key == "max_file_size") {
            log_.max_file_size = static_cast<size_t>(to_int(value));
        } else if (key == "max_files") {
            log_.max_files = static_cast<size_t>(to_int(value));
        }
    } else if (section == "smtp") {
        if (key == "bind_address" || key == "address") {
            smtp_.bind_address = value;
        } else if (key == "port") {
            smtp_.port = static_cast<uint16_t>(to_int(value));
        } else if (key == "tls_port") {
            smtp_.tls_port = static_cast<uint16_t>(to_int(value));
        } else if (key == "hostname") {
            smtp_.hostname = value;
        } else if (key == "max_connections") {
            smtp_.max_connections = static_cast<size_t>(to_int(value));
        } else if (key == "max_message_size") {
            smtp_.max_message_size = static_cast<size_t>(to_int(value));
        } else if (key == "max_recipients") {
            smtp_.max_recipients = static_cast<size_t>(to_int(value));
        } else if (key == "require_auth") {
            smtp_.require_auth = to_bool(value);
        } else if (key == "allow_relay") {
            smtp_.allow_relay = to_bool(value);
        } else if (key == "enable_starttls") {
            smtp_.enable_starttls = to_bool(value);
        } else if (key == "local_domains") {
            // Parse comma-separated list
            std::istringstream iss(value);
            std::string domain;
            while (std::getline(iss, domain, ',')) {
                auto start = domain.find_first_not_of(" \t");
                auto end = domain.find_last_not_of(" \t");
                if (start != std::string::npos && end != std::string::npos) {
                    smtp_.local_domains.push_back(domain.substr(start, end - start + 1));
                }
            }
        }
    } else if (section == "pop3") {
        if (key == "bind_address" || key == "address") {
            pop3_.bind_address = value;
        } else if (key == "port") {
            pop3_.port = static_cast<uint16_t>(to_int(value));
        } else if (key == "tls_port") {
            pop3_.tls_port = static_cast<uint16_t>(to_int(value));
        } else if (key == "max_connections") {
            pop3_.max_connections = static_cast<size_t>(to_int(value));
        } else if (key == "enable_starttls") {
            pop3_.enable_starttls = to_bool(value);
        }
    } else if (section == "imap") {
        if (key == "bind_address" || key == "address") {
            imap_.bind_address = value;
        } else if (key == "port") {
            imap_.port = static_cast<uint16_t>(to_int(value));
        } else if (key == "tls_port") {
            imap_.tls_port = static_cast<uint16_t>(to_int(value));
        } else if (key == "max_connections") {
            imap_.max_connections = static_cast<size_t>(to_int(value));
        } else if (key == "enable_starttls") {
            imap_.enable_starttls = to_bool(value);
        } else if (key == "max_search_results") {
            imap_.max_search_results = static_cast<size_t>(to_int(value));
        } else if (key == "enable_idle") {
            imap_.enable_idle = to_bool(value);
        }
    } else {
        // Store in custom values
        std::string full_key = section.empty() ? key : section + "." + key;
        custom_values_[full_key] = value;
    }
}

std::optional<std::string> Config::get(const std::string& key) const {
    auto it = custom_values_.find(key);
    if (it != custom_values_.end()) {
        return it->second;
    }
    return std::nullopt;
}

void Config::set(const std::string& key, const std::string& value) {
    custom_values_[key] = value;
}

}  // namespace email
