#pragma once
#include <boost/asio.hpp>
#include <message_queue.hpp>
#include <unordered_map>
#include <string>
#include <memory>

// Broker handles the connection of clients on the different ports
// decouples publishers from subs
class Broker {
public:
    // io_context: async operations
    // pub_port and sub_port fixed channels for publishers and subscribers
    Broker(boost::asio::io_context& io_context,
        unsigned short pub_port,
        unsigned short sub_port);

    // Start accepting connections
    void start();

private:
    void do_accept_publisher();
    void do_accept_subscriber();

    boost::asio::io_context& io_context_;
    // listens for publishers
    boost::asio::ip::tcp::acceptor pub_acceptor_;
    // listens for subscribers
    boost::asio:ip:tcp:acceptor sub_acceptor_;
    // serialize handlers
    boost::assio:strand<boost::asio::io_context::executor_type> strand_;

    // map channel names to queues
    std::unordered_map<std::string,
        std::shared_ptr<MessageQueue<std::string>>> channels_;
};