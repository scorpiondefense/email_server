#pragma once

#include <string>
#include <vector>
#include <optional>
#include <chrono>
#include <filesystem>
#include <memory>
#include <boost/asio.hpp>

namespace email::smtp {

namespace asio = boost::asio;
using tcp = asio::ip::tcp;

struct DeliveryResult {
    bool success = false;
    int reply_code = 0;
    std::string reply_message;
    std::string error;
};

struct MXRecord {
    std::string hostname;
    int priority;

    bool operator<(const MXRecord& other) const {
        return priority < other.priority;
    }
};

class SMTPRelay {
public:
    SMTPRelay(asio::io_context& io_context);

    // Local delivery to maildir
    bool deliver_local(const std::string& recipient_domain,
                       const std::string& recipient_local,
                       const std::string& message_content,
                       const std::filesystem::path& maildir_root);

    // Remote delivery via SMTP
    DeliveryResult deliver_remote(const std::string& recipient,
                                  const std::string& sender,
                                  const std::string& message_content);

    // DNS MX lookup
    std::vector<MXRecord> lookup_mx(const std::string& domain);

    // Queue management
    bool queue_message(const std::string& sender,
                       const std::vector<std::string>& recipients,
                       const std::string& content);

    void process_queue();

    void set_hostname(const std::string& hostname) { hostname_ = hostname; }
    void set_retry_interval(std::chrono::seconds interval) { retry_interval_ = interval; }
    void set_max_retries(int retries) { max_retries_ = retries; }

private:
    DeliveryResult send_to_server(const std::string& server,
                                  uint16_t port,
                                  const std::string& sender,
                                  const std::string& recipient,
                                  const std::string& content);

    std::string read_response(tcp::socket& socket);
    bool send_command(tcp::socket& socket, const std::string& command);
    bool expect_code(const std::string& response, int expected_code);

    asio::io_context& io_context_;
    std::string hostname_ = "localhost";
    std::chrono::seconds retry_interval_{300};
    int max_retries_ = 3;
};

// Message queue entry
struct QueuedMessage {
    std::string id;
    std::string sender;
    std::vector<std::string> recipients;
    std::string content;
    std::chrono::system_clock::time_point queued_at;
    std::chrono::system_clock::time_point next_retry;
    int retry_count = 0;
    std::string last_error;
};

}  // namespace email::smtp
