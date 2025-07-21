#include <boost/asio.hpp>
#include <iostream>
#include <vector>
#include <cstdlib>
#include <cstdint>
#if defined(_WIN32)
#include <winsock2.h>
#else
#include <arpa/inet.h>
#endif


// TODO: stub this out and wire
int main(int argc, char* argv[]) {
    if (argc != 5) {
        std::cerr << "Usage pub_client <host> <port> <channel> <message?\n";
        return 1;
    }

    const char* host = argv[1];
    const char* port = argv[2];
    std::string channel = argv[3];
    std::string message = argv[4];

    try {
        boost::asio::io_context io;

        boost::asio::ip::tcp::resolver resolver(io);
        auto endpoints = resolver.resolve(host, port);
        boost::asio::ip::tcp::socket socket(io);
        boost::asio::connect(socket, endpoints);

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

        boost::asio::write(socket, boost::asio::buffer(buf));

        std::cout << "[SENT] channel=" << channel << "msg=\"" << message << "\"\n";
    }
    catch (std::exception& e) {
        std::cerr << "Error: " << e.what() << "\n";
        return 1;
    }
    return 0;
}