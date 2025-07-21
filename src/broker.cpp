#include "core/broker.hpp"
#include "core/mutex_queue.hpp"
#include <iostream>
#include <cstdlib>
#if defined(_WIN32)
#include <winsock2.h>
#else
#include <arpa/inet.h>
#endif
#include <boost/asio/bind_executor.hpp>

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
                // on accept, create and start a session to handle framing
            auto session = std::make_shared<PublisherSession>(std::move(socket), strand_, channels_);
            // keep the session alive across all async ops
            session->start();
            }
            // Continue accepting next publisher
            do_accept_publisher();
        });
}

void Broker::PublisherSession::read_header() {
    buffer_.resize(header_size);
    auto self = shared_from_this();
    boost::asio::async_read(
        socket_,
        boost::asio::buffer(buffer_),
        boost::asio::transfer_exactly(buffer_.size()),
        boost::asio::bind_executor(strand_,[this,self](boost::system::error_code ec, std::size_t) {
            if (ec) return;
            // parse the network order lengths
            uint16_t ch_len = ntohs(*reinterpret_cast<uint16_t*>(buffer_.data()));
            uint32_t pl_len = ntohl(*reinterpret_cast<uint32_t*>(buffer_.data() + sizeof(uint16_t)));
            read_body(ch_len, pl_len);
        }));
}

void Broker::PublisherSession::read_body(uint16_t channel_len, uint32_t payload_len) {
    buffer_.resize(channel_len + payload_len);
    auto self = shared_from_this();
    boost::asio::async_read(
        socket_,
        boost::asio::buffer(buffer_),
        boost::asio::transfer_exactly(buffer_.size()),
        boost::asio::bind_executor(strand_,[this, self, channel_len](boost::system::error_code ec, std::size_t /*n*/){
            if (ec) return;
            // extract channel name and payload
            std::string channel(buffer_.data(), channel_len);
            std::string payload(buffer_.data() + channel_len, buffer_.size() - channel_len);
            // create the queue
            if (!channels_[channel]) {
                channels_[channel] = std::make_shared<MutexQueue<std::string>>();
            }
            channels_[channel]->push(payload);
            // continue with the next message
            read_header();
        }));
    }

// Asynchronously accept subscriber connections and re-arm
void Broker::do_accept_subscriber() {
    sub_acceptor_.async_accept(
        strand_,
        [this](boost::system::error_code ec, boost::asio::ip::tcp::socket sock) {
            if (!ec) {
                auto session = std::make_shared<SubscriberSession>(
                    std::move(sock),
                    strand_,
                    channels_);
                session->start();
            }
            // Continue accepting next subscriber
            do_accept_subscriber();
        });
    }

void Broker::SubscriberSession::read_subscription() {
    buffer_.resize(sizeof(uint16_t));
    auto self = shared_from_this();
    boost::asio::async_read(socket_,
        boost::asio::buffer(buffer_),
        [this, self](auto ec, std::size_t) {
            if (ec) return;
            uint16_t name_len = ntohs(*reinterpret_cast<uint16_t*>(buffer_.data()));
            buffer_.resize(name_len);
            boost::asio::async_read(socket_,
                boost::asio::buffer(buffer_),
                boost::asio::bind_executor(strand_,
                    [this, self, name_len](auto ec2, std::size_t) {
                        if (ec2) return;
                        std::string channel(buffer_.data(), name_len);
                        queue_ = channels_[channel];
                        if (!queue_) {
                            queue_ = std::make_shared<MutexQueue<std::string>>();
                            channels_[channel] = queue_;
                        }
                        deliver_next();
                    }));
                });
            }

void Broker::SubscriberSession::deliver_next() {
    auto self = shared_from_this();
    boost::asio::post(strand_, [this, self] {
        auto opt = queue_->wait_and_pop();
        if (!opt) return;
        const std::string& msg = *opt;
        uint32_t len = static_cast<uint32_t>(msg.size());
        std::vector<boost::asio::const_buffer> bufs;
        uint16_t netlen = htons(len);
        bufs.push_back(boost::asio::buffer(&netlen, sizeof(netlen)));
        bufs.push_back(boost::asio::buffer(msg));
        boost::asio::async_write(socket_, bufs,
            boost::asio::bind_executor(strand_,
                [this, self](auto ec, std::size_t) {
                    if (ec) return;
                    deliver_next();
                }));
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