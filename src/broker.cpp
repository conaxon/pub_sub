#include "core/broker.hpp"
#include <iostream>

Broker::Broker(boost::asio::io_context& io_context,
                unsigned short pub_port,
                unsigned short sub_port,)
        : io_context_(io_context),
          pub_acceptor_(io_context, {boost::asio::ip::tcp:v4(), pub_port}),
          sub_acceptor_(io_context, {boost::asio::ip::tcp::v4(), sub_port}),
          strand_(io_context.get_executor())
{
        // so we will start the acceptors before the async ops
        start();
}

void Broker::start() {
    do_accept_publishers();
    do_accept_subscribers();
}

void Broker::do_accept_publishers() {
    // async accept avoids blocking and leverages the asio concurrency model
    pub_acceptor_.async_accept(
        boost::asio::make_strand(io_context_),
        [this](boost::asio::system::error_code ec, boost::asio::ip::tcp::socket socket) {
            if (!ec) {
                // TODO: read messages, parse channel header, enqueue chanels
            }
            do_accept_publishers();
        });
    }

void Broker::do_accept_subscribers() {
    sub_acceptor_.async_accept(
        strand_,
        [this](boost::asio::system::error_code ec, boost::asio::ip::tcp::socket socket) {
            if (!ec) {
                // TODO: read messages, parse channel header, enqueue chanels
            }
            do_accept_subscribers();
        });

int main(int argc, char* argv[]) {
    if (argc != 3) {
        std::cerr << "usage: broker <pub_port> <sub_port>\n";
        return 1;
    }
    unsigned short pub_port = static_cast<unsigned short>(std::atoi(argv[1]));
    unsigned short sub_port = static_cast<unsigned short>(std::atoi(argv[2]));

    boost::asio::io_context io_context;
    Broker broker(io_context, pub_port, sub_port);
    // enter the io event loop and dispatch async handlers
    io_context.run();
    return 0;
    }

