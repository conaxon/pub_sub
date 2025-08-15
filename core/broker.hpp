// filename: broker.hpp
#pragma once
#include <boost/asio.hpp>
#include <message_queue.hpp>
#include <unordered_map>
#include <string>
#include <memory>
#include <vector>
#include <cstdint>
#include <cstdlib>
#include <algorithm>
#include <core/mutex_queue.hpp>
#include <core/lf_queue.hpp>
#include <core/spsc_queue_adapter.hpp>
#include <core/queue_factory.hpp>
#include <core/broker_context.hpp>
#include <condition_variable>
#include <mutex>

class Broker {
public:
    // io_context: async operations
    // pub_port and sub_port fixed channels for publishers and subscribers
    Broker(boost::asio::io_context& io_context,
        unsigned short pub_port,
        unsigned short sub_port,
        QueueKind kind,
        std::shared_ptr<BrokerContext> ctx);

    ~Broker();

    // Start accepting connections
    void start();

private:
    void do_accept_publisher();
    void do_accept_subscriber();
    void setup_acceptor(boost::asio::ip::tcp::acceptor& acc,
                        unsigned short port,
                        const char* label);

    static constexpr unsigned kBackoffBaseMs = 10;
    static constexpr unsigned kBackoffMaxMs = 1000;

    bool use_lf_;

    boost::asio::io_context& io_context_;
    // listens for publishers
    boost::asio::ip::tcp::acceptor pub_acceptor_;
    // listens for subscribers
    boost::asio::ip::tcp::acceptor sub_acceptor_;
    // serialize handlers
    boost::asio::strand<boost::asio::io_context::executor_type> strand_;
    // define the kind of queue to use
    QueueKind queue_kind_;

    std::shared_ptr<BrokerContext> ctx_;

    // map channel names to queues
    std::unordered_map<std::string,
        std::shared_ptr<MessageQueue<std::string>>> channels_;

    boost::asio::steady_timer pub_acceptor_retry_timer_;
    boost::asio::steady_timer sub_acceptor_retry_timer_;
    unsigned pub_backoff_pow2_{0};
    unsigned sub_backoff_pow2_{0};

    // session per connection handling
    class PublisherSession : public std::enable_shared_from_this<PublisherSession> {
    public:
        PublisherSession(boost::asio::ip::tcp::socket socket,
                        boost::asio::strand<boost::asio::io_context::executor_type> strand,
                        std::unordered_map<std::string, std::shared_ptr<MessageQueue<std::string>>>& channels,
                        QueueKind kind, std::shared_ptr<BrokerContext> ctx)
        : socket_(std::move(socket)),
          strand_(strand),
          channels_(channels),
          kind_(kind),
          ctx_(std::move(ctx)) {
            boost::system::error_code ip_ec;
            auto ep = socket_.remote_endpoint(ip_ec);
            peer_ip_ = ip_ec ? std::string("<unknown>") : ep.address().to_string();
          }
        
        void start() { read_header(); }

    private:
        void read_header();
        void read_body(uint16_t channel_len, uint32_t payload_len);
        void fail(const char* where, const boost::system::error_code& ec);
        void reject_malformed(const char* where, uint16_t ch_len, uint32_t pl_len);
        
        boost::asio::ip::tcp::socket socket_;
        boost::asio::strand<boost::asio::io_context::executor_type> strand_;
        std::unordered_map<std::string,
            std::shared_ptr<MessageQueue<std::string>>>& channels_;
        std::vector<char> buffer_;
        static constexpr size_t header_size = sizeof(uint16_t) + sizeof(uint32_t);
        QueueKind kind_;
        std::shared_ptr<BrokerContext> ctx_;
        std::string peer_ip_;
        uint32_t malformed_frames_{0};
    };

    class SubscriberSession:
    public
        std::enable_shared_from_this<SubscriberSession> {  
            public:
                SubscriberSession(boost::asio::ip::tcp::socket socket,
                boost::asio::strand<boost::asio::io_context::executor_type> strand,
                std::unordered_map<std::string,
                    std::shared_ptr<MessageQueue<std::string>>>& channels, QueueKind kind,
                    std::shared_ptr<BrokerContext> ctx):
                        socket_(std::move(socket)),
                        strand_(strand),
                        channels_(channels),
                        kind_(kind),
                        ctx_(std::move(ctx)) {}

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
        QueueKind kind_;
        std::shared_ptr<BrokerContext> ctx_;
        std::shared_ptr<MessageQueue<std::string>> queue_;
        std::vector<char> buffer_;
        std::atomic<bool> stopped_{false};
        std::atomic<bool> worker_running_{false};
        std::atomic<uint64_t> worker_gen_{0};
    };
};