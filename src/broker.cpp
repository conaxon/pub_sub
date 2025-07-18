#include "core/broker.hpp"
#include <iostream>
#include <cstdlib>

// Constructs the broker, binds acceptors, and initializes the strand
Broker::Broker(boost::asio::io_context& io_context,
               unsigned short pub_port,
               unsigned short sub_port)
    : io_context_(io_context),
      pub_acceptor_(io_context, {boost::asio::ip::tcp::v4(), pub_port}),
      sub_acceptor_(io_context, {boost::asio::ip::tcp::v4(), sub_port}),
      strand_(io_context.get_executor())
{
    // Start accepting connections before entering the event loop
    start();
}

void Broker::start() {
    do_accept_publisher();
    do_accept_subscriber();
}

// Asynchronously accept publisher connections and re-arm
void Broker::do_accept_publisher() {
    pub_acceptor_.async_accept(
        boost::asio::make_strand(io_context_),
        [this](boost::system::error_code ec, boost::asio::ip::tcp::socket socket) {
            if (!ec) {
                // TODO: Read framed message, parse channel header, enqueue into channels_[channel]
            }
            // Continue accepting next publisher
            do_accept_publisher();
        });
}

// Asynchronously accept subscriber connections and re-arm
void Broker::do_accept_subscriber() {
    sub_acceptor_.async_accept(
        strand_,
        [this](boost::system::error_code ec, boost::asio::ip::tcp::socket socket) {
            if (!ec) {
                // TODO: Read subscription request, and then loop popping from channels_[channel] to write to socket
            }
            // Continue accepting next subscriber
            do_accept_subscriber();
        });
}

int main(int argc, char* argv[]) {
    if (argc != 3) {
        std::cerr << "Usage: broker <pub_port> <sub_port>";
        return 1;
    }
    unsigned short pub_port = static_cast<unsigned short>(std::atoi(argv[1]));
    unsigned short sub_port = static_cast<unsigned short>(std::atoi(argv[2]));

    boost::asio::io_context io_context;
    Broker broker(io_context, pub_port, sub_port);
    // Enter the I/O event loop, dispatching asynchronous handlers
    io_context.run();
    return 0;
}