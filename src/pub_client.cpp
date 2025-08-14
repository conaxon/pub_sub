// filename: src/pub_client.cpp
#include <boost/asio.hpp>
#include <iostream>
#include <vector>
#include <cstdlib>
#include <cstdint>
#include <thread>
#include <chrono>
#if defined(_WIN32)
#include <winsock2.h>
#else
#include <arpa/inet.h>
#endif


int main(int argc, char* argv[]) {
    if (argc != 5) {
        std::cerr << "Usage pub_client <host> <port> <channel> <message>\n";
        return 1;
    }

    const char* host = argv[1];
    const char* port = argv[2];
    std::string channel = argv[3];
    std::string message = argv[4];

    try {
        boost::asio::io_context io;
        boost::asio::ip::tcp::resolver resolver(io);
        auto endpoints = resolver.resolve(boost::asio::ip::tcp::v4(),host, port);
        boost::asio::ip::tcp::socket socket(io);
        boost::system::error_code ec;
        const int max_attempts = 6;
        const int base_backoff_ms = 200;
        const int max_backoff_ms = 5000;
        bool connected = false;

        for (int attempt = 1; attempt <= max_attempts; ++attempt) {
            boost::asio::ip::tcp::resolver resolver(io);
            auto results = resolver.resolve(boost::asio::ip::tcp::v4(), host, port, ec);

            if (ec) {
                std::cerr << "[pub] resolve failed (attempt " << attempt << "/" << max_attempts
                << "): " << ec.message() << "\n";
            } else {
                socket = boost::asio::ip::tcp::socket(io);
                boost::asio::connect(socket, results, ec);
                if (!ec) { connected = true; break; }
                std::cerr << "[pub] connect failed (attempt " << attempt << "/" << max_attempts
                          << " ): " << ec.message() << "\n";
            }
            int backoff = std::min(max_backoff_ms, base_backoff_ms << (attempt - 1));
            std::this_thread::sleep_for(std::chrono::milliseconds(backoff));    
        }
        if (!connected) {
            std::cerr << "[pub] connect failed: " << ec.message() << "\n";
            return 1;
        }

        std::cout << "[pub] connected\n";

        uint16_t ch_len = htons(static_cast<uint16_t>(channel.size()));
        uint32_t msg_len = htonl(static_cast<uint32_t>(message.size()));
        std::vector<char> buf;
        buf.reserve(2 + 4 + channel.size() + message.size());

        buf.insert(buf.end(),
            reinterpret_cast<char*>(&ch_len),
            reinterpret_cast<char*>(&ch_len) + sizeof(ch_len));
        buf.insert(buf.end(),
            reinterpret_cast<char*>(&msg_len),
            reinterpret_cast<char*>(&msg_len) + sizeof(msg_len));
        buf.insert(buf.end(), channel.begin(), channel.end());
        buf.insert(buf.end(), message.begin(), message.end());

        std::size_t n = boost::asio::write(socket, boost::asio::buffer(buf), ec);

        if (ec) {
            std::cerr << "[pub] write failed: " << ec.message() << "\n";
            return 1;
        }

        std::cout << "[SENT] bytes=" << n
            << " channel=" << channel
            << " msg=\"" << message << "\"\n";
    }
    catch (std::exception& e) {
        std::cerr << "Error: " << e.what() << "\n";
        return 1;
    }
    return 0;
}