#pragma once

#include <memory>
#include <string>
#include <filesystem>
#include <functional>

#ifdef ENABLE_TLS
#include <boost/asio/ssl.hpp>
#endif

namespace email {

#ifdef ENABLE_TLS
namespace ssl = boost::asio::ssl;
#endif

class SSLContext {
public:
    enum class Mode {
        Server,
        Client
    };

    enum class Protocol {
        TLS_1_2,
        TLS_1_3,
        TLS_Auto  // Negotiate highest available
    };

    SSLContext(Mode mode = Mode::Server, Protocol protocol = Protocol::TLS_Auto);
    ~SSLContext();

    SSLContext(const SSLContext&) = delete;
    SSLContext& operator=(const SSLContext&) = delete;
    SSLContext(SSLContext&&) noexcept;
    SSLContext& operator=(SSLContext&&) noexcept;

    bool load_certificate(const std::filesystem::path& cert_file);
    bool load_private_key(const std::filesystem::path& key_file,
                          const std::string& password = "");
    bool load_certificate_chain(const std::filesystem::path& chain_file);
    bool load_ca_file(const std::filesystem::path& ca_file);
    bool load_ca_path(const std::filesystem::path& ca_path);

    void set_verify_mode(bool verify_peer, bool fail_if_no_cert = false);
    void set_ciphers(const std::string& cipher_list);
    void set_password_callback(std::function<std::string()> callback);

    bool is_initialized() const { return initialized_; }
    std::string last_error() const { return last_error_; }

#ifdef ENABLE_TLS
    ssl::context& native() { return *context_; }
    const ssl::context& native() const { return *context_; }
#endif

    static SSLContext create_server_context(
        const std::filesystem::path& cert_file,
        const std::filesystem::path& key_file,
        const std::filesystem::path& ca_file = "",
        const std::string& ciphers = "");

    static SSLContext create_client_context(
        const std::filesystem::path& ca_file = "",
        bool verify_server = true);

private:
    void set_error(const std::string& msg);

#ifdef ENABLE_TLS
    std::unique_ptr<ssl::context> context_;
#endif
    bool initialized_ = false;
    std::string last_error_;
    Mode mode_;
    Protocol protocol_;
    std::function<std::string()> password_callback_;
};

}  // namespace email
