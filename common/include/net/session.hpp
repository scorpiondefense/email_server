#pragma once

#include <memory>
#include <string>
#include <deque>
#include <functional>
#include <chrono>
#include <variant>
#include <boost/asio.hpp>
#ifdef ENABLE_TLS
#include <boost/asio/ssl.hpp>
#endif

namespace email {

namespace asio = boost::asio;
using tcp = asio::ip::tcp;
#ifdef ENABLE_TLS
namespace ssl = asio::ssl;
#endif

class Session : public std::enable_shared_from_this<Session> {
public:
#ifdef ENABLE_TLS
    using PlainSocket = tcp::socket;
    using SSLSocket = ssl::stream<tcp::socket>;
    using Socket = std::variant<PlainSocket, SSLSocket>;
#else
    using Socket = tcp::socket;
#endif

    Session(asio::io_context& io_context, tcp::socket socket);
#ifdef ENABLE_TLS
    Session(asio::io_context& io_context, tcp::socket socket, ssl::context& ssl_ctx);
#endif
    virtual ~Session() = default;

    Session(const Session&) = delete;
    Session& operator=(const Session&) = delete;

    virtual void start();
    virtual void stop();

    void send(const std::string& data);
    void send_line(const std::string& line);

    bool is_tls() const { return is_tls_; }
    std::string remote_address() const;
    uint16_t remote_port() const;

    void set_timeout(std::chrono::seconds timeout);
    void reset_timeout();

    bool is_authenticated() const { return authenticated_; }
    const std::string& username() const { return username_; }
    const std::string& domain() const { return domain_; }

    void set_authenticated(bool auth) { authenticated_ = auth; }
    void set_username(const std::string& user) { username_ = user; }
    void set_domain(const std::string& dom) { domain_ = dom; }

#ifdef ENABLE_TLS
    void start_tls(ssl::context& ssl_ctx);
#endif

protected:
    virtual void on_connect();
    virtual void on_data(const std::string& data) = 0;
    virtual void on_line(const std::string& line);
    virtual void on_disconnect();
    virtual void on_error(const boost::system::error_code& ec);
    virtual void on_tls_handshake_complete();

    void do_read();
    void do_write();

    void close_socket();

    asio::io_context& io_context_;
    Socket socket_;
    bool is_tls_ = false;

    asio::streambuf read_buffer_;
    std::deque<std::string> write_queue_;
    bool writing_ = false;

    asio::steady_timer timeout_timer_;
    std::chrono::seconds timeout_{300};

    bool authenticated_ = false;
    std::string username_;
    std::string domain_;

    bool stopped_ = false;

private:
    void handle_read(const boost::system::error_code& ec, std::size_t bytes_transferred);
    void handle_write(const boost::system::error_code& ec, std::size_t bytes_transferred);
    void handle_timeout(const boost::system::error_code& ec);

#ifdef ENABLE_TLS
    template<typename SocketType>
    void do_read_impl(SocketType& socket);

    template<typename SocketType>
    void do_write_impl(SocketType& socket);
#endif
};

}  // namespace email
