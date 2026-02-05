#include "net/session.hpp"
#include "logger.hpp"

namespace email {

Session::Session(asio::io_context& io_context, tcp::socket socket)
    : io_context_(io_context)
#ifdef ENABLE_TLS
    , socket_(std::move(socket))
#else
    , socket_(std::move(socket))
#endif
    , timeout_timer_(io_context) {
}

#ifdef ENABLE_TLS
Session::Session(asio::io_context& io_context, tcp::socket socket, ssl::context& ssl_ctx)
    : io_context_(io_context)
    , socket_(SSLSocket(std::move(socket), ssl_ctx))
    , is_tls_(true)
    , timeout_timer_(io_context) {
}
#endif

void Session::start() {
    on_connect();
    reset_timeout();

#ifdef ENABLE_TLS
    if (is_tls_) {
        auto self = shared_from_this();
        std::get<SSLSocket>(socket_).async_handshake(
            ssl::stream_base::server,
            [this, self](const boost::system::error_code& ec) {
                if (!ec) {
                    on_tls_handshake_complete();
                    do_read();
                } else {
                    on_error(ec);
                }
            });
    } else {
        do_read();
    }
#else
    do_read();
#endif
}

void Session::stop() {
    if (stopped_) return;
    stopped_ = true;

    timeout_timer_.cancel();
    on_disconnect();
    close_socket();
}

void Session::close_socket() {
    boost::system::error_code ec;

#ifdef ENABLE_TLS
    if (is_tls_) {
        auto& ssl_socket = std::get<SSLSocket>(socket_);
        ssl_socket.lowest_layer().close(ec);
    } else {
        std::get<PlainSocket>(socket_).close(ec);
    }
#else
    socket_.close(ec);
#endif
}

std::string Session::remote_address() const {
    try {
#ifdef ENABLE_TLS
        if (is_tls_) {
            return std::get<SSLSocket>(socket_).lowest_layer().remote_endpoint().address().to_string();
        }
        return std::get<PlainSocket>(socket_).remote_endpoint().address().to_string();
#else
        return socket_.remote_endpoint().address().to_string();
#endif
    } catch (...) {
        return "unknown";
    }
}

uint16_t Session::remote_port() const {
    try {
#ifdef ENABLE_TLS
        if (is_tls_) {
            return std::get<SSLSocket>(socket_).lowest_layer().remote_endpoint().port();
        }
        return std::get<PlainSocket>(socket_).remote_endpoint().port();
#else
        return socket_.remote_endpoint().port();
#endif
    } catch (...) {
        return 0;
    }
}

void Session::set_timeout(std::chrono::seconds timeout) {
    timeout_ = timeout;
}

void Session::reset_timeout() {
    timeout_timer_.cancel();
    timeout_timer_.expires_after(timeout_);
    auto self = shared_from_this();
    timeout_timer_.async_wait([this, self](const boost::system::error_code& ec) {
        handle_timeout(ec);
    });
}

void Session::handle_timeout(const boost::system::error_code& ec) {
    if (ec == asio::error::operation_aborted) {
        return;  // Timer was cancelled
    }
    if (!stopped_) {
        LOG_DEBUG("Session timeout");
        stop();
    }
}

void Session::send(const std::string& data) {
    auto self = shared_from_this();
    asio::post(io_context_, [this, self, data]() {
        bool was_empty = write_queue_.empty();
        write_queue_.push_back(data);
        if (was_empty) {
            do_write();
        }
    });
}

void Session::send_line(const std::string& line) {
    send(line + "\r\n");
}

void Session::do_read() {
    if (stopped_) return;

    auto self = shared_from_this();

#ifdef ENABLE_TLS
    if (is_tls_) {
        do_read_impl(std::get<SSLSocket>(socket_));
    } else {
        do_read_impl(std::get<PlainSocket>(socket_));
    }
#else
    asio::async_read_until(
        socket_,
        read_buffer_,
        "\r\n",
        [this, self](const boost::system::error_code& ec, std::size_t bytes_transferred) {
            handle_read(ec, bytes_transferred);
        });
#endif
}

#ifdef ENABLE_TLS
template<typename SocketType>
void Session::do_read_impl(SocketType& socket) {
    auto self = shared_from_this();
    asio::async_read_until(
        socket,
        read_buffer_,
        "\r\n",
        [this, self](const boost::system::error_code& ec, std::size_t bytes_transferred) {
            handle_read(ec, bytes_transferred);
        });
}
#endif

void Session::handle_read(const boost::system::error_code& ec, std::size_t bytes_transferred) {
    if (stopped_) return;

    if (!ec) {
        reset_timeout();

        std::istream is(&read_buffer_);
        std::string line;
        std::getline(is, line);

        // Remove trailing \r if present
        if (!line.empty() && line.back() == '\r') {
            line.pop_back();
        }

        on_line(line);

        do_read();
    } else {
        on_error(ec);
        stop();
    }
}

void Session::do_write() {
    if (stopped_ || write_queue_.empty()) return;

    auto self = shared_from_this();

#ifdef ENABLE_TLS
    if (is_tls_) {
        do_write_impl(std::get<SSLSocket>(socket_));
    } else {
        do_write_impl(std::get<PlainSocket>(socket_));
    }
#else
    asio::async_write(
        socket_,
        asio::buffer(write_queue_.front()),
        [this, self](const boost::system::error_code& ec, std::size_t bytes_transferred) {
            handle_write(ec, bytes_transferred);
        });
#endif
}

#ifdef ENABLE_TLS
template<typename SocketType>
void Session::do_write_impl(SocketType& socket) {
    auto self = shared_from_this();
    asio::async_write(
        socket,
        asio::buffer(write_queue_.front()),
        [this, self](const boost::system::error_code& ec, std::size_t bytes_transferred) {
            handle_write(ec, bytes_transferred);
        });
}
#endif

void Session::handle_write(const boost::system::error_code& ec, std::size_t /* bytes_transferred */) {
    if (stopped_) return;

    if (!ec) {
        write_queue_.pop_front();
        if (!write_queue_.empty()) {
            do_write();
        }
    } else {
        on_error(ec);
        stop();
    }
}

void Session::on_connect() {
    LOG_DEBUG_FMT("New connection from {}:{}", remote_address(), remote_port());
}

void Session::on_line(const std::string& line) {
    on_data(line);
}

void Session::on_disconnect() {
    LOG_DEBUG_FMT("Connection closed from {}:{}", remote_address(), remote_port());
}

void Session::on_error(const boost::system::error_code& ec) {
    if (ec == asio::error::eof || ec == asio::error::connection_reset) {
        return;  // Normal disconnection
    }
    LOG_ERROR_FMT("Session error: {}", ec.message());
}

void Session::on_tls_handshake_complete() {
    LOG_DEBUG("TLS handshake completed");
}

#ifdef ENABLE_TLS
void Session::start_tls(ssl::context& ssl_ctx) {
    if (is_tls_) return;  // Already TLS

    auto& plain_socket = std::get<PlainSocket>(socket_);
    socket_ = SSLSocket(std::move(plain_socket), ssl_ctx);
    is_tls_ = true;

    auto self = shared_from_this();
    std::get<SSLSocket>(socket_).async_handshake(
        ssl::stream_base::server,
        [this, self](const boost::system::error_code& ec) {
            if (!ec) {
                on_tls_handshake_complete();
            } else {
                on_error(ec);
                stop();
            }
        });
}
#endif

}  // namespace email
