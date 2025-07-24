#include <boost/asio.hpp>
#include <iostream>
#include <vector>
#include <cstdint>
#include <chrono>
#if defined(_WIN32)
#include <winsock2.h>
#else
#include <arpa/inet.h>
#endif

int main(int argc, char* argv[]) {
    if (argc != 6) {
        std::cerr << "Usage: bench_pub <host> <port> <channel> <msg_size> <count>\n";
        return 1;
    }
    const char* host = argv[1];
    const char* port = argv[2];
    std::string channel = argv[3];
    size_t msg_size = std::stoul(argv[4]);
    size_t count = std::stoul(argv[5]);

    std::string payload(msg_size, 'X');

    boost::asio::io_context io;
    boost::asio::ip::tcp::resolver resolver(io);
    auto endpoints = resolver.resolve(boost::asio::ip::tcp::v4(), host, port);
    boost::asio::ip::tcp::socket sock(io);
    boost::asio::connect(sock, endpoints);

    uint16_t ch_len = htons(static_cast<uint16_t>(channel.size()));
    uint32_t msg_len_net = htonl(static_cast<uint32_t>(payload.size()));

    std::vector<char> header;
    header.reserve(2+4+channel.size());
    header.insert(header.end(), (char*)&ch_len, (char*)&ch_len + 2);
    header.insert(header.end(), (char*)&msg_len_net, (char*)&msg_len_net + 4);
    header.insert(header.end(), channel.begin(), channel.end());

    auto start = std::chrono::steady_clock::now();
    for(size_t i=0;i<count;i++) {
        boost::asio::write(sock, boost::asio::buffer(header));
        boost::asio::write(sock, boost::asio::buffer(payload));
    }
    auto end = std::chrono::steady_clock::now();

    auto dur_ms = std::chrono::duration_cast<std::chrono::milliseconds>(end-start).count();
    double mps = (double)count / (dur_ms/1000.0);
    std::cout << "sent=" << count << " msgs in " << dur_ms << " ms => " << mps << " msg/s\n";
}