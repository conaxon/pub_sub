#include <boost/asio.hpp>
#include <iostream>
#include <vector>
#include <cstdlib>
#include <cstdlib>

#if defined(_WIN32)
  #include <winsock2.h>
#else
  #include <arpa/inet.h>
#endif

using boost::asio::ip::tcp;

int main(int argc, char* argv[]) {
    if (argc != 4){
        std::cerr << "Usage: sub_client <host> <port> <channel>\n";
        return 1;
    }

    const char* host = argv[1];
    const char* port = argv[2];
    std::string channel = argv[3];

    boost::asio::io_context io_context;
    tcp::resolver resolver(io_context);
    auto endpoints = resolver.resolve(host,port);
    tcp::socket socket(io_context);
    boost::asio::connect(socket,endpoints);

    uint16_t ch_len = static_cast<uint16_t>(channel.size());
    uint16_t net_len = htons(ch_len);
    boost::asio::write(socket,boost::asio::buffer(&net_len, sizeof(net_len)));
    boost::asio::write(socket,boost::asio::buffer(channel));

    std::cout << "Subscribed to channel '" << channel << "'....\n";

    for (;;) {
        uint16_t payload_len_net;
        boost::asio::read(socket, boost::asio::buffer(&payload_len_net, sizeof(payload_len_net)));
        uint16_t payload_len = ntohs(payload_len_net);

        std::vector<char> buf(payload_len);
        boost::asio::read(socket, boost::asio::buffer(buf));

        std::string message(buf.begin(), buf.end());
        std::cout << "[RECV] " << message << std::endl;
    }

    return 0;
}