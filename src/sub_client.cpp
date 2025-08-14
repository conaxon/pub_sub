// filename: src/sub_client.cpp
#include <boost/asio.hpp>
#include <iostream>
#include <vector>
#include <cstdlib>
#include <cstdlib>
#include <thread>
#include <chrono>

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
  tcp::socket socket(io_context);
  boost::system::error_code ec;
  const int max_attempts = 6;
  const int base_backoff_ms = 200;
  const int max_backoff_ms = 5000;
  bool connected = false;
  for (int attempt = 1; attempt <= max_attempts; ++attempt) {
    tcp::resolver resolver(io_context);
    auto results = resolver.resolve(host, port, ec);
    if (ec) {
      std::cerr << "[sub] resolve failed (attempt " << attempt << "/" << max_attempts
                << "): " << ec.message() << "\n";
    } else {
      socket = tcp::socket(io_context);
      boost::asio::connect(socket, results, ec);
      if (!ec) { connected = true; break; }
        std::cerr << "[sub] connect failed (attempt " << attempt << "/" << max_attempts
                  << "): " << ec.message() << "\n";
                }
      int backoff = std::min(max_backoff_ms, base_backoff_ms << (attempt - 1));\
      std::this_thread::sleep_for(std::chrono::milliseconds(backoff));
    }
    if (!connected) {
      std::cerr << "[sub] giving up after " << max_attempts << " attempts\n";
      return 1;
    }

    uint16_t ch_len = static_cast<uint16_t>(channel.size());
    uint16_t net_len = htons(ch_len);
    boost::asio::write(socket,boost::asio::buffer(&net_len, sizeof(net_len)));
    boost::asio::write(socket,boost::asio::buffer(channel));

    std::cout << "Subscribed to channel '" << channel << "'....\n";
    
    try {
      for (;;) {
        uint32_t payload_len_net;
        boost::asio::read(socket, boost::asio::buffer(&payload_len_net, sizeof(payload_len_net)));
        uint32_t payload_len = ntohl(payload_len_net);

        std::vector<char> buf(payload_len);
        boost::asio::read(socket, boost::asio::buffer(buf));

        std::string message(buf.begin(), buf.end());
        std::cout << "[RECV] " << message << std::endl;
      }
    }
    catch (const boost::system::system_error& e) {
      if (e.code() == boost::asio::error::eof) {
        std::cout << "*** Server closed connection, exiting\n";
      } else {
        std::cerr << "*** Error: " << e.what() << "\n";
      }
    }
  }