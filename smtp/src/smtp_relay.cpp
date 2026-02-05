#include "smtp_relay.hpp"
#include "storage/maildir.hpp"
#include "logger.hpp"
#include <sstream>
#include <random>
#include <chrono>
#include <fstream>

// For DNS resolution (simplified - in production use a proper DNS library)
#include <netdb.h>
#include <arpa/inet.h>
#include <resolv.h>
#include <arpa/nameser.h>

namespace email::smtp {

SMTPRelay::SMTPRelay(asio::io_context& io_context)
    : io_context_(io_context) {
}

bool SMTPRelay::deliver_local(const std::string& recipient_domain,
                              const std::string& recipient_local,
                              const std::string& message_content,
                              const std::filesystem::path& maildir_root) {
    Maildir maildir(maildir_root, recipient_domain, recipient_local);

    if (!maildir.exists()) {
        if (!maildir.initialize()) {
            LOG_ERROR_FMT("Failed to initialize maildir for {}@{}", recipient_local, recipient_domain);
            return false;
        }
    }

    std::string unique_id = maildir.deliver(message_content, "INBOX");
    if (unique_id.empty()) {
        LOG_ERROR_FMT("Failed to deliver message to {}@{}", recipient_local, recipient_domain);
        return false;
    }

    LOG_INFO_FMT("Delivered message to {}@{}", recipient_local, recipient_domain);
    return true;
}

DeliveryResult SMTPRelay::deliver_remote(const std::string& recipient,
                                         const std::string& sender,
                                         const std::string& message_content) {
    DeliveryResult result;

    // Extract domain from recipient
    auto at_pos = recipient.find('@');
    if (at_pos == std::string::npos) {
        result.error = "Invalid recipient address";
        return result;
    }

    std::string domain = recipient.substr(at_pos + 1);

    // Look up MX records
    auto mx_records = lookup_mx(domain);
    if (mx_records.empty()) {
        // Fall back to A record
        mx_records.push_back({domain, 0});
    }

    // Try each MX in priority order
    for (const auto& mx : mx_records) {
        result = send_to_server(mx.hostname, 25, sender, recipient, message_content);
        if (result.success) {
            break;
        }
    }

    return result;
}

std::vector<MXRecord> SMTPRelay::lookup_mx(const std::string& domain) {
    std::vector<MXRecord> records;

    unsigned char answer[NS_PACKETSZ];
    int len = res_query(domain.c_str(), ns_c_in, ns_t_mx, answer, sizeof(answer));

    if (len < 0) {
        LOG_WARNING_FMT("MX lookup failed for {}", domain);
        return records;
    }

    ns_msg msg;
    if (ns_initparse(answer, len, &msg) < 0) {
        return records;
    }

    int count = ns_msg_count(msg, ns_s_an);
    for (int i = 0; i < count; ++i) {
        ns_rr rr;
        if (ns_parserr(&msg, ns_s_an, i, &rr) < 0) {
            continue;
        }

        if (ns_rr_type(rr) != ns_t_mx) {
            continue;
        }

        const unsigned char* rdata = ns_rr_rdata(rr);
        int priority = ns_get16(rdata);

        char exchange[NS_MAXDNAME];
        if (dn_expand(answer, answer + len, rdata + 2, exchange, sizeof(exchange)) < 0) {
            continue;
        }

        records.push_back({exchange, priority});
    }

    // Sort by priority
    std::sort(records.begin(), records.end());

    return records;
}

DeliveryResult SMTPRelay::send_to_server(const std::string& server,
                                         uint16_t port,
                                         const std::string& sender,
                                         const std::string& recipient,
                                         const std::string& content) {
    DeliveryResult result;

    try {
        tcp::resolver resolver(io_context_);
        auto endpoints = resolver.resolve(server, std::to_string(port));

        tcp::socket socket(io_context_);
        boost::asio::connect(socket, endpoints);

        // Read greeting
        std::string response = read_response(socket);
        if (!expect_code(response, 220)) {
            result.error = "Server rejected connection: " + response;
            return result;
        }

        // EHLO
        send_command(socket, "EHLO " + hostname_);
        response = read_response(socket);
        if (!expect_code(response, 250)) {
            // Try HELO
            send_command(socket, "HELO " + hostname_);
            response = read_response(socket);
            if (!expect_code(response, 250)) {
                result.error = "HELO rejected: " + response;
                return result;
            }
        }

        // MAIL FROM
        send_command(socket, "MAIL FROM:<" + sender + ">");
        response = read_response(socket);
        if (!expect_code(response, 250)) {
            result.error = "MAIL FROM rejected: " + response;
            return result;
        }

        // RCPT TO
        send_command(socket, "RCPT TO:<" + recipient + ">");
        response = read_response(socket);
        if (!expect_code(response, 250) && !expect_code(response, 251)) {
            result.error = "RCPT TO rejected: " + response;
            return result;
        }

        // DATA
        send_command(socket, "DATA");
        response = read_response(socket);
        if (!expect_code(response, 354)) {
            result.error = "DATA rejected: " + response;
            return result;
        }

        // Send message content
        std::istringstream iss(content);
        std::string line;
        while (std::getline(iss, line)) {
            // Remove trailing \r if present
            if (!line.empty() && line.back() == '\r') {
                line.pop_back();
            }
            // Byte-stuff lines starting with .
            if (!line.empty() && line[0] == '.') {
                line = "." + line;
            }
            send_command(socket, line);
        }

        // End of data
        send_command(socket, ".");
        response = read_response(socket);
        if (!expect_code(response, 250)) {
            result.error = "Message rejected: " + response;
            return result;
        }

        // QUIT
        send_command(socket, "QUIT");
        read_response(socket);  // Ignore response

        result.success = true;
        result.reply_code = 250;
        result.reply_message = "Message delivered";

    } catch (const std::exception& e) {
        result.error = e.what();
    }

    return result;
}

std::string SMTPRelay::read_response(tcp::socket& socket) {
    boost::asio::streambuf buffer;
    std::string response;

    while (true) {
        boost::asio::read_until(socket, buffer, "\r\n");

        std::istream is(&buffer);
        std::string line;
        std::getline(is, line);

        if (!line.empty() && line.back() == '\r') {
            line.pop_back();
        }

        response += line;

        // Check if this is the last line (no continuation)
        if (line.length() >= 4 && line[3] == ' ') {
            break;
        }

        response += "\n";
    }

    return response;
}

bool SMTPRelay::send_command(tcp::socket& socket, const std::string& command) {
    std::string cmd = command + "\r\n";
    boost::asio::write(socket, boost::asio::buffer(cmd));
    return true;
}

bool SMTPRelay::expect_code(const std::string& response, int expected_code) {
    if (response.length() < 3) {
        return false;
    }

    try {
        int code = std::stoi(response.substr(0, 3));
        return code == expected_code;
    } catch (...) {
        return false;
    }
}

bool SMTPRelay::queue_message(const std::string& /* sender */,
                              const std::vector<std::string>& /* recipients */,
                              const std::string& /* content */) {
    // Queue implementation would store messages for later delivery
    // This is a placeholder for a full queue implementation
    return true;
}

void SMTPRelay::process_queue() {
    // Process queued messages
    // This would be called periodically by a background thread
}

}  // namespace email::smtp
