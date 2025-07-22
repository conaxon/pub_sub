// filename: broker.hpp
#pragma once
#include <boost/asio.hpp>
#include <message_queue.hpp>
#include <unordered_map>
#include <string>
#include <memory>
#include <vector>
#include <cstdint>

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
    boost::asio::ip::tcp::acceptor sub_acceptor_;
    // serialize handlers
    boost::asio::strand<boost::asio::io_context::executor_type> strand_;

    // map channel names to queues
    std::unordered_map<std::string,
        std::shared_ptr<MessageQueue<std::string>>> channels_;

    // session per connection handling
    class PublisherSession : public std::enable_shared_from_this<PublisherSession> {
    public:
        PublisherSession(boost::asio::ip::tcp::socket socket,
                        boost::asio::strand<boost::asio::io_context::executor_type> strand,
                        std::unordered_map<std::string, std::shared_ptr<MessageQueue<std::string>>>& channels)
                        : socket_(std::move(socket)), strand_(strand), channels_(channels) {}
        
        void start() { read_header(); }

    private:
        void read_header();
        void read_body(uint16_t channel_len, uint32_t payload_len);

        boost::asio::ip::tcp::socket socket_;
        boost::asio::strand<boost::asio::io_context::executor_type> strand_;
        std::unordered_map<std::string,
            std::shared_ptr<MessageQueue<std::string>>>& channels_;
        std::vector<char> buffer_;
        static constexpr size_t header_size = sizeof(uint16_t) + sizeof(uint32_t);
    };

    class SubscriberSession:
    public
        std::enable_shared_from_this<SubscriberSession> {  
            public:
                SubscriberSession(boost::asio::ip::tcp::socket socket,
                boost::asio::strand<boost::asio::io_context::executor_type> strand,
                std::unordered_map<std::string,
                    std::shared_ptr<MessageQueue<std::string>>>& channels):
                        socket_(std::move(socket)),
                        strand_(strand),
                        channels_(channels)
                    {}
            ~SubscriberSession();
            void start() { read_subscription(); }
            
    private:
        void read_subscription();
        void deliver_next();
        void launch_worker();
        void async_send(std::string msg);
    
        boost::asio::ip::tcp::socket socket_;
        boost::asio::strand<boost::asio::io_context::executor_type> strand_;
        std::unordered_map<std::string,
        std::shared_ptr<MessageQueue<std::string>>>& channels_;
        std::shared_ptr<MessageQueue<std::string>> queue_;
        std::vector<char> buffer_;
        std::atomic<bool> stopped_{false};
    };
};