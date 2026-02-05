#include "ssl_context.hpp"
#include "logger.hpp"

#ifdef ENABLE_TLS
#include <openssl/err.h>
#include <openssl/ssl.h>
#endif

namespace email {

SSLContext::SSLContext(Mode mode, Protocol protocol)
    : mode_(mode), protocol_(protocol) {
#ifdef ENABLE_TLS
    ssl::context::method method;

    if (mode == Mode::Server) {
        switch (protocol) {
            case Protocol::TLS_1_2:
                method = ssl::context::tlsv12_server;
                break;
            case Protocol::TLS_1_3:
                method = ssl::context::tlsv13_server;
                break;
            case Protocol::TLS_Auto:
            default:
                method = ssl::context::tls_server;
                break;
        }
    } else {
        switch (protocol) {
            case Protocol::TLS_1_2:
                method = ssl::context::tlsv12_client;
                break;
            case Protocol::TLS_1_3:
                method = ssl::context::tlsv13_client;
                break;
            case Protocol::TLS_Auto:
            default:
                method = ssl::context::tls_client;
                break;
        }
    }

    context_ = std::make_unique<ssl::context>(method);

    // Set default options
    context_->set_options(
        ssl::context::default_workarounds |
        ssl::context::no_sslv2 |
        ssl::context::no_sslv3 |
        ssl::context::no_tlsv1 |
        ssl::context::no_tlsv1_1 |
        ssl::context::single_dh_use
    );
#endif
}

SSLContext::~SSLContext() = default;

SSLContext::SSLContext(SSLContext&& other) noexcept
    : initialized_(other.initialized_)
    , last_error_(std::move(other.last_error_))
    , mode_(other.mode_)
    , protocol_(other.protocol_)
    , password_callback_(std::move(other.password_callback_)) {
#ifdef ENABLE_TLS
    context_ = std::move(other.context_);
#endif
    other.initialized_ = false;
}

SSLContext& SSLContext::operator=(SSLContext&& other) noexcept {
    if (this != &other) {
#ifdef ENABLE_TLS
        context_ = std::move(other.context_);
#endif
        initialized_ = other.initialized_;
        last_error_ = std::move(other.last_error_);
        mode_ = other.mode_;
        protocol_ = other.protocol_;
        password_callback_ = std::move(other.password_callback_);
        other.initialized_ = false;
    }
    return *this;
}

bool SSLContext::load_certificate(const std::filesystem::path& cert_file) {
#ifdef ENABLE_TLS
    if (!context_) {
        set_error("SSL context not initialized");
        return false;
    }

    try {
        context_->use_certificate_file(cert_file.string(), ssl::context::pem);
        return true;
    } catch (const boost::system::system_error& e) {
        set_error(std::string("Failed to load certificate: ") + e.what());
        return false;
    }
#else
    (void)cert_file;
    set_error("TLS support not enabled");
    return false;
#endif
}

bool SSLContext::load_private_key(const std::filesystem::path& key_file,
                                   const std::string& password) {
#ifdef ENABLE_TLS
    if (!context_) {
        set_error("SSL context not initialized");
        return false;
    }

    try {
        if (!password.empty()) {
            context_->set_password_callback(
                [password](std::size_t, ssl::context::password_purpose) {
                    return password;
                });
        } else if (password_callback_) {
            context_->set_password_callback(
                [this](std::size_t, ssl::context::password_purpose) {
                    return password_callback_();
                });
        }

        context_->use_private_key_file(key_file.string(), ssl::context::pem);
        initialized_ = true;
        return true;
    } catch (const boost::system::system_error& e) {
        set_error(std::string("Failed to load private key: ") + e.what());
        return false;
    }
#else
    (void)key_file;
    (void)password;
    set_error("TLS support not enabled");
    return false;
#endif
}

bool SSLContext::load_certificate_chain(const std::filesystem::path& chain_file) {
#ifdef ENABLE_TLS
    if (!context_) {
        set_error("SSL context not initialized");
        return false;
    }

    try {
        context_->use_certificate_chain_file(chain_file.string());
        return true;
    } catch (const boost::system::system_error& e) {
        set_error(std::string("Failed to load certificate chain: ") + e.what());
        return false;
    }
#else
    (void)chain_file;
    set_error("TLS support not enabled");
    return false;
#endif
}

bool SSLContext::load_ca_file(const std::filesystem::path& ca_file) {
#ifdef ENABLE_TLS
    if (!context_) {
        set_error("SSL context not initialized");
        return false;
    }

    try {
        context_->load_verify_file(ca_file.string());
        return true;
    } catch (const boost::system::system_error& e) {
        set_error(std::string("Failed to load CA file: ") + e.what());
        return false;
    }
#else
    (void)ca_file;
    set_error("TLS support not enabled");
    return false;
#endif
}

bool SSLContext::load_ca_path(const std::filesystem::path& ca_path) {
#ifdef ENABLE_TLS
    if (!context_) {
        set_error("SSL context not initialized");
        return false;
    }

    try {
        context_->add_verify_path(ca_path.string());
        return true;
    } catch (const boost::system::system_error& e) {
        set_error(std::string("Failed to load CA path: ") + e.what());
        return false;
    }
#else
    (void)ca_path;
    set_error("TLS support not enabled");
    return false;
#endif
}

void SSLContext::set_verify_mode(bool verify_peer, bool fail_if_no_cert) {
#ifdef ENABLE_TLS
    if (!context_) return;

    ssl::verify_mode mode = ssl::verify_none;
    if (verify_peer) {
        mode = ssl::verify_peer;
        if (fail_if_no_cert) {
            mode |= ssl::verify_fail_if_no_peer_cert;
        }
    }
    context_->set_verify_mode(mode);
#else
    (void)verify_peer;
    (void)fail_if_no_cert;
#endif
}

void SSLContext::set_ciphers(const std::string& cipher_list) {
#ifdef ENABLE_TLS
    if (!context_) return;

    SSL_CTX_set_cipher_list(context_->native_handle(), cipher_list.c_str());
#else
    (void)cipher_list;
#endif
}

void SSLContext::set_password_callback(std::function<std::string()> callback) {
    password_callback_ = std::move(callback);
}

void SSLContext::set_error(const std::string& msg) {
    last_error_ = msg;
#ifdef ENABLE_TLS
    unsigned long err;
    while ((err = ERR_get_error()) != 0) {
        char buf[256];
        ERR_error_string_n(err, buf, sizeof(buf));
        last_error_ += std::string(" [") + buf + "]";
    }
#endif
    LOG_ERROR(last_error_);
}

SSLContext SSLContext::create_server_context(
    const std::filesystem::path& cert_file,
    const std::filesystem::path& key_file,
    const std::filesystem::path& ca_file,
    const std::string& ciphers) {

    SSLContext ctx(Mode::Server, Protocol::TLS_Auto);

    if (!ctx.load_certificate(cert_file)) {
        return ctx;
    }

    if (!ctx.load_private_key(key_file)) {
        return ctx;
    }

    if (!ca_file.empty()) {
        ctx.load_ca_file(ca_file);
    }

    if (!ciphers.empty()) {
        ctx.set_ciphers(ciphers);
    }

    return ctx;
}

SSLContext SSLContext::create_client_context(
    const std::filesystem::path& ca_file,
    bool verify_server) {

    SSLContext ctx(Mode::Client, Protocol::TLS_Auto);

    if (!ca_file.empty()) {
        ctx.load_ca_file(ca_file);
    }

    ctx.set_verify_mode(verify_server);
    ctx.initialized_ = true;

    return ctx;
}

}  // namespace email
