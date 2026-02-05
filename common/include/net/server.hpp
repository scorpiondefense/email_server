#pragma once

#include <memory>
#include <string>
#include <vector>
#include <thread>
#include <atomic>
#include <functional>
#include <set>
#include <mutex>
#include <boost/asio.hpp>
#ifdef ENABLE_TLS
#include <boost/asio/ssl.hpp>
#endif

#include "session.hpp"

namespace email {

namespace asio = boost::asio;
using tcp = asio::ip::tcp;
#ifdef ENABLE_TLS
namespace ssl = asio::ssl;
#endif

template<typename SessionType>
class Server {
public:
    Server(const std::string& name, const std::string& bind_address,
           uint16_t port, size_t thread_count = 4);

#ifdef ENABLE_TLS
    Server(const std::string& name, const std::string& bind_address,
           uint16_t port, ssl::context& ssl_ctx, size_t thread_count = 4);
#endif

    virtual ~Server();

    Server(const Server&) = delete;
    Server& operator=(const Server&) = delete;

    void start();
    void stop();

    bool is_running() const { return running_; }
    size_t connection_count() const;

    void set_max_connections(size_t max) { max_connections_ = max; }
    void set_connection_timeout(std::chrono::seconds timeout) { connection_timeout_ = timeout; }

    // Callback for session creation customization
    using SessionFactory = std::function<std::shared_ptr<SessionType>(
        asio::io_context&, tcp::socket, ssl::context*)>;
    void set_session_factory(SessionFactory factory) { session_factory_ = factory; }

protected:
    virtual std::shared_ptr<SessionType> create_session(
        asio::io_context& io_ctx, tcp::socket socket);

#ifdef ENABLE_TLS
    virtual std::shared_ptr<SessionType> create_tls_session(
        asio::io_context& io_ctx, tcp::socket socket, ssl::context& ssl_ctx);
#endif

    virtual void on_session_start(std::shared_ptr<SessionType> session);
    virtual void on_session_end(std::shared_ptr<SessionType> session);

private:
    void do_accept();
    void remove_session(std::shared_ptr<SessionType> session);

    std::string name_;
    std::string bind_address_;
    uint16_t port_;

    asio::io_context io_context_;
    tcp::acceptor acceptor_;
    std::vector<std::thread> threads_;

#ifdef ENABLE_TLS
    ssl::context* ssl_context_ = nullptr;
    bool use_tls_ = false;
#endif

    std::atomic<bool> running_{false};
    size_t thread_count_;
    size_t max_connections_ = 1000;
    std::chrono::seconds connection_timeout_{300};

    std::set<std::shared_ptr<SessionType>> sessions_;
    std::mutex sessions_mutex_;

    SessionFactory session_factory_;
};

// Template implementation

template<typename SessionType>
Server<SessionType>::Server(const std::string& name, const std::string& bind_address,
                            uint16_t port, size_t thread_count)
    : name_(name)
    , bind_address_(bind_address)
    , port_(port)
    , acceptor_(io_context_)
    , thread_count_(thread_count) {
}

#ifdef ENABLE_TLS
template<typename SessionType>
Server<SessionType>::Server(const std::string& name, const std::string& bind_address,
                            uint16_t port, ssl::context& ssl_ctx, size_t thread_count)
    : name_(name)
    , bind_address_(bind_address)
    , port_(port)
    , acceptor_(io_context_)
    , ssl_context_(&ssl_ctx)
    , use_tls_(true)
    , thread_count_(thread_count) {
}
#endif

template<typename SessionType>
Server<SessionType>::~Server() {
    stop();
}

template<typename SessionType>
void Server<SessionType>::start() {
    if (running_) return;

    tcp::endpoint endpoint(asio::ip::make_address(bind_address_), port_);
    acceptor_.open(endpoint.protocol());
    acceptor_.set_option(asio::socket_base::reuse_address(true));
    acceptor_.bind(endpoint);
    acceptor_.listen(asio::socket_base::max_listen_connections);

    running_ = true;
    do_accept();

    for (size_t i = 0; i < thread_count_; ++i) {
        threads_.emplace_back([this]() {
            io_context_.run();
        });
    }
}

template<typename SessionType>
void Server<SessionType>::stop() {
    if (!running_) return;

    running_ = false;
    io_context_.stop();

    {
        std::lock_guard<std::mutex> lock(sessions_mutex_);
        for (auto& session : sessions_) {
            session->stop();
        }
        sessions_.clear();
    }

    for (auto& thread : threads_) {
        if (thread.joinable()) {
            thread.join();
        }
    }
    threads_.clear();
}

template<typename SessionType>
size_t Server<SessionType>::connection_count() const {
    std::lock_guard<std::mutex> lock(const_cast<std::mutex&>(sessions_mutex_));
    return sessions_.size();
}

template<typename SessionType>
void Server<SessionType>::do_accept() {
    acceptor_.async_accept(
        [this](boost::system::error_code ec, tcp::socket socket) {
            if (!ec && running_) {
                if (connection_count() < max_connections_) {
                    std::shared_ptr<SessionType> session;

#ifdef ENABLE_TLS
                    if (use_tls_ && ssl_context_) {
                        session = create_tls_session(io_context_, std::move(socket), *ssl_context_);
                    } else {
                        session = create_session(io_context_, std::move(socket));
                    }
#else
                    session = create_session(io_context_, std::move(socket));
#endif

                    if (session) {
                        {
                            std::lock_guard<std::mutex> lock(sessions_mutex_);
                            sessions_.insert(session);
                        }
                        session->set_timeout(connection_timeout_);
                        on_session_start(session);
                        session->start();
                    }
                }
            }

            if (running_) {
                do_accept();
            }
        });
}

template<typename SessionType>
std::shared_ptr<SessionType> Server<SessionType>::create_session(
    asio::io_context& io_ctx, tcp::socket socket) {
    if (session_factory_) {
        return session_factory_(io_ctx, std::move(socket), nullptr);
    }
    return std::make_shared<SessionType>(io_ctx, std::move(socket));
}

#ifdef ENABLE_TLS
template<typename SessionType>
std::shared_ptr<SessionType> Server<SessionType>::create_tls_session(
    asio::io_context& io_ctx, tcp::socket socket, ssl::context& ssl_ctx) {
    if (session_factory_) {
        return session_factory_(io_ctx, std::move(socket), &ssl_ctx);
    }
    return std::make_shared<SessionType>(io_ctx, std::move(socket), ssl_ctx);
}
#endif

template<typename SessionType>
void Server<SessionType>::on_session_start(std::shared_ptr<SessionType> /* session */) {
    // Override in derived class if needed
}

template<typename SessionType>
void Server<SessionType>::on_session_end(std::shared_ptr<SessionType> session) {
    remove_session(session);
}

template<typename SessionType>
void Server<SessionType>::remove_session(std::shared_ptr<SessionType> session) {
    std::lock_guard<std::mutex> lock(sessions_mutex_);
    sessions_.erase(session);
}

}  // namespace email
