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
    if (argc != 5) {
        std::cerr << "Usage: bench_sub <host> <port> <channel> <count>\n";
        return 1;
    }
    const char* host = argv[1];
    const char* port = argv[2];
    std::string channel = argv[3];
    size_t target = std::stoul(argv[4]);

    boost::asio::io_context io;
    using tcp = boost::asio::ip::tcp;
    tcp::resolver resolver(io);
    auto endpoints = resolver.resolve(host, port);
    tcp::socket sock(io);
    boost::asio::connect(sock, endpoints);

    uint16_t ch_len = htons((uint16_t)channel.size());
    boost::asio::write(sock, boost::asio::buffer(&ch_len, sizeof(ch_len)));
    boost::asio::write(sock, boost::asio::buffer(channel));

    uint64_t received = 0;
    auto start = std::chrono::steady_clock::now();
    while (received < target) {
        uint32_t net_len;
        boost::asio::read(sock, boost::asio::buffer(&net_len, sizeof(net_len)));
        uint32_t len = ntohl(net_len);
        std::vector<char> buf(len);
        boost::asio::read(sock, boost::asio::buffer(buf));
        received++;
    }
    auto end = std::chrono::steady_clock::now();
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(end-start).count();
    std::cout << "recv=" << received << " msgs in " << ms << " ms => "
        << (received/(ms/1000.0)) << " msg/s\n";
    
    return 0;
}