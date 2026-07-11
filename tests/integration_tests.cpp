#include "mailforge/config/ServerConfig.hpp"
#include "mailforge/logging/Logger.hpp"
#include "mailforge/network/TcpListener.hpp"

#include <boost/asio/io_context.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/connect.hpp>
#include <boost/asio/read_until.hpp>
#include <boost/asio/write.hpp>
#include <boost/asio/streambuf.hpp>

#include <cassert>
#include <iostream>
#include <string>
#include <thread>
#include <chrono>

using boost::asio::ip::tcp;

namespace {

std::string read_response(tcp::socket& socket) {
    boost::asio::streambuf buf;
    boost::asio::read_until(socket, buf, "\r\n");
    std::string response(
        boost::asio::buffers_begin(buf.data()),
        boost::asio::buffers_begin(buf.data()) + buf.size()
    );
    return response;
}

void send_command(tcp::socket& socket, const std::string& command) {
    boost::asio::write(socket, boost::asio::buffer(command + "\r\n"));
}

} // namespace

int main() {
    mailforge::config::ServerConfig config;
    config.host = "127.0.0.1";
    config.port = 2526; // Use a different port to avoid conflicts
    config.server_name = "mailforge.local";
    config.mail_directory = std::filesystem::temp_directory_path() / "mailforge-integration-tests";
    
    mailforge::logging::Logger logger(mailforge::logging::Level::info);
    
    boost::asio::io_context io_context;
    mailforge::network::TcpListener listener(io_context, config, logger);
    listener.start();

    // Run io_context on a background thread
    std::thread server_thread([&io_context]() {
        io_context.run();
    });

    // Wait a brief moment for the listener to start accepting
    std::this_thread::sleep_for(std::chrono::milliseconds(200));

    try {
        tcp::socket socket(io_context);
        tcp::resolver resolver(io_context);
        boost::asio::connect(socket, resolver.resolve("127.0.0.1", "2526"));

        // 1. Initial Greeting
        std::string greeting = read_response(socket);
        assert(greeting.rfind("220", 0) == 0);

        // 2. EHLO
        send_command(socket, "EHLO test.client");
        std::string ehlo_resp = read_response(socket);
        assert(ehlo_resp.rfind("250", 0) == 0);

        // 3. MAIL FROM
        send_command(socket, "MAIL FROM:<alice@example.com>");
        std::string mail_resp = read_response(socket);
        assert(mail_resp.rfind("250", 0) == 0);

        // 4. RCPT TO
        send_command(socket, "RCPT TO:<bob@mailforge.local>");
        std::string rcpt_resp = read_response(socket);
        assert(rcpt_resp.rfind("250", 0) == 0);

        // 5. DATA
        send_command(socket, "DATA");
        std::string data_resp = read_response(socket);
        assert(data_resp.rfind("354", 0) == 0);

        // Send content
        send_command(socket, "Subject: Hello Integration Test\r\n\r\nThis is a test message.\r\n.");
        std::string end_resp = read_response(socket);
        assert(end_resp.rfind("250", 0) == 0);

        // QUIT
        send_command(socket, "QUIT");
        std::string quit_resp = read_response(socket);
        assert(quit_resp.rfind("221", 0) == 0);

        std::cout << "[SUCCESS] Integration test passed successfully!" << std::endl;
    } catch (const std::exception& e) {
        std::cerr << "[FAIL] Integration test failed: " << e.what() << std::endl;
        io_context.stop();
        server_thread.join();
        return 1;
    }

    io_context.stop();
    server_thread.join();
    return 0;
}
