// filename: src/broker.cpp
#include <core/broker.hpp>
#include <core/mutex_queue.hpp>
#include <iostream>
#include <cstdlib>
#if defined(_WIN32)
#include <winsock2.h>
#else
#include <arpa/inet.h>
#endif
#include <boost/asio/bind_executor.hpp>
#include <spsc_queue_adapter.hpp>
#include <core/thread_pool.hpp>
#include <lf_queue.hpp>
#include <core/queue_factory.hpp>
#include <chrono>
#include <core/broker_context.hpp>
#include <boost/asio/ip/v6_only.hpp>
#include <boost/asio/socket_base.hpp>
#include <charconv>
#include <cstring>

static std::size_t pick_pool_size() {
    const std::size_t hw = std::thread::hardware_concurrency() ? 
                           std::thread::hardware_concurrency() : 1;
    std::size_t max_cap = std::min<std::size_t>(hw * 4, 256);
    std::size_t n = hw;

    if (const char* env = std::getenv("BROKER_THREADS")) {
        // trim spaces
        const char* b = env; while (*b && std::isspace(static_cast<unsigned char>(*b))) ++b;
        const char* e = env + std::strlen(env);
        while (e > b && std::isspace(static_cast<unsigned char>(e[-1]))) --e;

        std::uint64_t v = 0;
        auto res = std::from_chars(b, e, v, 10);
        if (res.ec != std::errc{} || res.ptr != e || v == 0) {
            std::cerr << "[broker] ignoring BROKER_THREADS (not a positive integer): '" << env << "'\n";
        } else {
            n = static_cast<std::size_t>(v);
            if (n > max_cap) {
                std::cerr << "[broker] BROKER_THREADS=" << n
                          << " too large; clamping to " << max_cap << "\n";
                n = max_cap;
            }
        }
    }
    return n;
}

ThreadPool g_pool(pick_pool_size());


namespace {
    constexpr std::size_t kMaxChannelLen = 256;
    constexpr std::size_t kMaxPayloadLen = 1 * 1024 *1024;
    constexpr std::size_t kMaxChannels = 1024;
}

// Constructs the broker, binds acceptors, and initializes the strand
Broker::Broker(boost::asio::io_context& io_context,
               unsigned short pub_port,
               unsigned short sub_port,
               QueueKind kind,
               std::shared_ptr<BrokerContext> ctx)
    : io_context_(io_context),
      pub_acceptor_(io_context),
      sub_acceptor_(io_context),
      strand_(io_context.get_executor()),
      queue_kind_(kind),
      ctx_(std::move(ctx)),
      pub_acceptor_retry_timer_(io_context),
      sub_acceptor_retry_timer_(io_context)
    
{
    setup_acceptor(pub_acceptor_, pub_port, "publisher");
    setup_acceptor(sub_acceptor_, sub_port, "subscriber");

    std::cout << "[broker] ctor: pub_port=" << pub_port
              << " sub_port=" << sub_port << "\n";
    // Start accepting connections before entering the event loop
    start();
}

Broker::~Broker() {
    boost::system::error_code ignore;
    pub_acceptor_retry_timer_.cancel();
    sub_acceptor_retry_timer_.cancel();
    pub_acceptor_.cancel(ignore);
    sub_acceptor_.cancel(ignore);
    pub_acceptor_.close(ignore);
    sub_acceptor_.close(ignore);
}

void Broker::setup_acceptor(boost::asio::ip::tcp::acceptor& acc,
                            unsigned short port,
                            const char* label)
{
    namespace asio = boost::asio;
    using tcp = asio::ip::tcp;

    boost::system::error_code ec;

    // Try dual-stack IPv6 first
    tcp::endpoint ep6(tcp::v6(), port);
    acc.open(ep6.protocol(), ec);
    if (!ec) {
        acc.set_option(asio::socket_base::reuse_address(true), ec); ec.clear();
        acc.set_option(asio::ip::v6_only(false), ec); ec.clear();
        acc.bind(ep6, ec);
        if (!ec) {
            acc.listen(asio::socket_base::max_listen_connections, ec);
            if (!ec) {
                asio::ip::v6_only v6only_opt;
                bool v6only_value = true;
                acc.get_option(v6only_opt, ec);
                if (!ec) v6only_value = v6only_opt.value();

                std::cout << "[broker] " << label
                          << " listening on [::]:" << port
                          << (v6only_value ? " (IPv6-only)" : " (dual-stack)")
                          << "\n";
                return;
            }
        }
        boost::system::error_code ignore;
        acc.close(ignore);
    }

    // Fallback to IPv4-only
    ec.clear();
    tcp::endpoint ep4(tcp::v4(), port);
    acc.open(ep4.protocol(), ec);
    if (ec) {
        std::cerr << "[broker] FAILED to open " << label
                  << " acceptor (v4): " << ec.message() << "\n";
        throw boost::system::system_error(ec);
    }
    acc.set_option(asio::socket_base::reuse_address(true), ec); ec.clear();
    acc.bind(ep4, ec);
    if (ec) {
        std::cerr << "[broker] FAILED to bind " << label
                  << " on 0.0.0.0:" << port << " (v4): " << ec.message() << "\n";
        throw boost::system::system_error(ec);
    }
    acc.listen(asio::socket_base::max_listen_connections, ec);
    if (ec) {
        std::cerr << "[broker] FAILED to listen " << label
                  << " (v4): " << ec.message() << "\n";
        throw boost::system::system_error(ec);
    }
    std::cout << "[broker] " << label
              << " listening on 0.0.0.0:" << port << " (IPv4-only fallback)\n";
}

void Broker::start() {
    do_accept_publisher();
    do_accept_subscriber();
}

// Asynchronously accept publisher connections and re-arm
void Broker::do_accept_publisher() {
    using tcp = boost::asio::ip::tcp;

    pub_acceptor_.async_accept(
        strand_,
        [this](const boost::system::error_code& ec, tcp::socket socket) {
            if (ec) {
                if (ec == boost::asio::error::operation_aborted) {
                    return; // shutting down
                }
                // backoff = min(kMaxMs, kBaseMs << pow2), with jitter ±25%
                pub_backoff_pow2_ = std::min<unsigned>(pub_backoff_pow2_ + 1, 7);
                unsigned delay_ms = std::min<unsigned>(kBackoffMaxMs,
                                                       kBackoffBaseMs << pub_backoff_pow2_);
                unsigned jitter = delay_ms / 4;
                unsigned rand0  = static_cast<unsigned>(std::rand()) % (2 * jitter + 1);
                unsigned delay_with_jitter = delay_ms - jitter + rand0;

                std::cerr << "[broker] accept publisher error: " << ec.message()
                          << " (retry in " << delay_with_jitter << " ms)\n";

                pub_acceptor_retry_timer_.expires_after(std::chrono::milliseconds(delay_with_jitter));
                pub_acceptor_retry_timer_.async_wait(
                    boost::asio::bind_executor(
                        strand_,
                        [this](const boost::system::error_code& tec) {
                            if (!tec) do_accept_publisher();
                        }));
                return;
            }

            // Success: reset backoff
            pub_backoff_pow2_ = 0;

            // Optional: IP ban check
            bool banned = false;
            {
                boost::system::error_code ep_ec;
                auto ep = socket.remote_endpoint(ep_ec);
                if (!ep_ec) {
                    const std::string ip = ep.address().to_string();
                    const auto now = std::chrono::steady_clock::now();
                    std::lock_guard<std::mutex> lk(ctx_->bad_peers_mu);
                    auto it = ctx_->bad_peers.find(ip);
                    if (it != ctx_->bad_peers.end() && it->second.until > now) {
                        banned = true;
                    } else if (it != ctx_->bad_peers.end() && it->second.until <= now) {
                        it->second.strikes = 0;
                    }
                }
            }
            if (banned) {
                std::cerr << "[broker] rejecting banned publisher\n";
                boost::system::error_code ignore;
                socket.shutdown(tcp::socket::shutdown_both, ignore);
                socket.close(ignore);
                do_accept_publisher();
                return;
            }

            std::cout << "[broker] publisher connected\n";
            // Create and start publisher session
            auto session = std::make_shared<PublisherSession>(
                std::move(socket), strand_, channels_, queue_kind_, ctx_);
            session->start();

            // Re-arm
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
        boost::asio::bind_executor(strand_, [this, self](boost::system::error_code ec, std::size_t) {
            if (ec) { fail("read_header", ec); return; }
            // parse the network order lengths
            uint16_t ch_len = ntohs(*reinterpret_cast<uint16_t*>(buffer_.data()));
            uint32_t pl_len = ntohl(*reinterpret_cast<uint32_t*>(buffer_.data() + sizeof(uint16_t)));
            
            if (ch_len == 0 || ch_len > kMaxChannelLen || pl_len > kMaxPayloadLen) {
                reject_malformed("read_header", ch_len, pl_len);
                return;
            }
            
            read_body(ch_len, pl_len);
        }));
}

void Broker::PublisherSession::read_body(uint16_t channel_len, uint32_t payload_len) {
    // buffer_.resize(channel_len + payload_len);
    if (channel_len == 0 || channel_len > kMaxChannelLen || payload_len > kMaxPayloadLen) {
        reject_malformed("read_body(recheck)", channel_len, payload_len);
        return;
    }
    buffer_.resize(static_cast<std::size_t>(channel_len) + static_cast<std::size_t>(payload_len));
    auto self = shared_from_this();
    boost::asio::async_read(
        socket_,
        boost::asio::buffer(buffer_),
        boost::asio::transfer_exactly(buffer_.size()),
        boost::asio::bind_executor(strand_, [this, self, channel_len](boost::system::error_code ec, std::size_t){
            if (ec) { fail("read_body", ec); return; }
            // extract channel name and payload
            std::string channel(buffer_.data(), channel_len);
            std::string payload(buffer_.data() + channel_len, buffer_.size() - channel_len);
            
            auto it = channels_.find(channel);
            bool exists = (it != channels_.end());

            if (!exists) {
                if (channels_.size() >= kMaxChannels) {
                    std::cerr << "[Broker] channel limit reached (" << channels_.size()
                            << "/" << kMaxChannels << ") — rejecting new channel '"
                            << channel << "'; dropping message\n";
                } else {
                    auto q = make_queue(kind_);
                    it = channels_.emplace(channel, std::move(q)).first;
                    exists = true;
                    std::cout << "[Broker] created channel '" << channel << "' ("
                            << channels_.size() << "/" << kMaxChannels << ")\n";
                }
            }

            if (exists) {
                std::cout << "[Broker] RX '" << payload << "' on channel '" << channel << "'\n";
                if (!it->second->try_push(payload)) {
                    std::cerr << "[Broker] DROP on channel '" << channel
                            << "': queue back-pressure (message_size=" << payload.size() << ")\n";
                }
            }
            // continue with the next message
            read_header();
        }));
    }

// Centralized error handling for PublisherSession async reads
void Broker::PublisherSession::fail(const char* where, const boost::system::error_code& ec) {
    // Unified, high-signal log (include numeric value for grepping)
    std::cerr << "[PublisherSession] " << where << " error: " << ec.message()
              << " (code=" << ec.value() << ")\n";
    // Best-effort shutdown then close; ignore errors if already closed.
    boost::system::error_code ignore;
    socket_.shutdown(boost::asio::ip::tcp::socket::shutdown_both, ignore);
    socket_.close(ignore);
    buffer_.clear();
    // Do NOT re-arm another read; let shared_ptr go out of scope naturally.
}

void Broker::PublisherSession::reject_malformed(const char* where, uint16_t ch_len, uint32_t pl_len) {
    ++malformed_frames_;
    if (ctx_) ctx_->metrics.malformed_frames.fetch_add(1, std::memory_order_relaxed);
    std::cerr << "[PublisherSession] " << where << " MALFORMED frame: "
              << "channel_len=" << ch_len << ", payload_len=" << pl_len
              << " (limits: ch<=" << kMaxChannelLen
              << ", payload<=" << kMaxPayloadLen << ")"
              << " [count=" << malformed_frames_ << "]\n";
    if (ctx_ && !peer_ip_.empty() && peer_ip_ != "<unknown>") {
        const auto now = std::chrono::steady_clock::now();
        bool just_banned = false;
        {
            std::lock_guard<std::mutex> lk(ctx_->bad_peers_mu);
            auto &entry = ctx_->bad_peers[peer_ip_];
            
            if (entry.until <= now) {
                entry.strikes = 0;
            }
            entry.strikes++;

            if (entry.strikes >= ctx_->bad_frame_threshold) {
                entry.until = now + ctx_->ban_duration;
                entry.strikes = 0;
                just_banned = true;
            }
        }
        if (just_banned) {
            std::cerr << "[PublisherSession] banning IP " << peer_ip_
                      << " for " << std::chrono::duration_cast<std::chrono::seconds>(ctx_->ban_duration).count()
                      << "s due to repeated malformed frames\n";
        }
    }
    boost::system::error_code ignore;
    socket_.shutdown(boost::asio::ip::tcp::socket::shutdown_both, ignore);
    socket_.close(ignore);
    buffer_.clear();
}

// Asynchronously accept subscriber connections and re-arm
void Broker::do_accept_subscriber() {
    using tcp = boost::asio::ip::tcp;
    sub_acceptor_.async_accept(
        strand_,
        [this](const boost::system::error_code& ec, tcp::socket sock) {
            if (ec) {
                if (ec == boost::asio::error::operation_aborted) {
                    return; // shutting down
                }
                sub_backoff_pow2_ = std::min<unsigned>(sub_backoff_pow2_ + 1, 7);
                unsigned delay_ms = std::min<unsigned>(kBackoffMaxMs,
                                                       kBackoffBaseMs << sub_backoff_pow2_);
                unsigned jitter = delay_ms / 4;
                unsigned rand0  = static_cast<unsigned>(std::rand()) % (2 * jitter + 1);
                unsigned delay_with_jitter = delay_ms - jitter + rand0;

                std::cerr << "[broker] accept subscriber error: " << ec.message()
                          << " (retry in " << delay_with_jitter << " ms)\n";

                sub_acceptor_retry_timer_.expires_after(std::chrono::milliseconds(delay_with_jitter));
                sub_acceptor_retry_timer_.async_wait(
                    boost::asio::bind_executor(
                        strand_,
                        [this](const boost::system::error_code& tec) {
                            if (!tec) do_accept_subscriber();
                        }));
                return;
            }

            // Success: reset backoff
            sub_backoff_pow2_ = 0;

            std::cout << "[broker] subscriber connected\n";
            auto session = std::make_shared<SubscriberSession>(
                std::move(sock), strand_, channels_, queue_kind_, ctx_);
            session->start();

            // Re-arm
            do_accept_subscriber();
        });
}

Broker::SubscriberSession::~SubscriberSession() {
    stopped_.store(true, std::memory_order_relaxed);
    if (queue_) queue_->close();

    bool was_running = worker_running_.load(std::memory_order_relaxed);
    if (was_running) {
        auto dl = std::chrono::steady_clock::now() + std::chrono::milliseconds(100);
        while (worker_running_.load(std::memory_order_relaxed) && 
              std::chrono::steady_clock::now() < dl) {
            std::this_thread::sleep_for(std::chrono::milliseconds(5));
        }
    }
    std::cerr << "[broker] SubscriberSession dtor: stop signaled; "
              << "worker_running=" << (worker_running_.load(std::memory_order_relaxed) ? "yes" : "no")
              << "\n";
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
                        auto it = channels_.find(channel);
                        bool exists = (it != channels_.end());

                        if (!exists) {
                            if (channels_.size() >= kMaxChannels) {
                                std::cerr << "[broker] channel limit reached (" << channels_.size()
                                        << "/" << kMaxChannels << ") — rejecting subscription to new channel '"
                                        << channel << "'\n";
                                stopped_ = true;
                                boost::system::error_code close_ec; socket_.close(close_ec);
                                return;
                            } else {
                                queue_ = std::make_shared<MutexQueue<std::string>>();
                                it = channels_.emplace(channel, queue_).first;
                                exists = true;
                                std::cout << "[broker] created channel '" << channel << "' ("
                                        << channels_.size() << "/" << kMaxChannels << ")\n";
                            }
                        } else {
                            queue_ = it->second;
                        }
                        //deliver_next();
                        launch_worker();
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

void Broker::SubscriberSession::launch_worker() {
    extern ThreadPool g_pool;
    auto self = shared_from_this();

    // New generation for this launch; clear the running flag.
    const uint64_t gen = worker_gen_.fetch_add(1, std::memory_order_relaxed) + 1;
    worker_running_.store(false, std::memory_order_relaxed);

    g_pool.post([self, gen]{
        // Announce "started" on the strand immediately.
        boost::asio::post(self->strand_, [self, gen]{
            // Ignore if a newer generation has since launched.
            if (gen == self->worker_gen_.load(std::memory_order_relaxed)) {
                self->worker_running_.store(true, std::memory_order_relaxed);
                // std::cout << "[broker] subscriber worker started (gen=" << gen << ")\n";
            }
        });

        // Main worker loop (off the IO thread).
        while (!self->stopped_) {
            auto opt = self->queue_->wait_and_pop();
            if (!opt) break; // queue closed or stopping
            auto msg = std::move(*opt);
            boost::asio::post(self->strand_, [self, msg = std::move(msg)]() mutable {
                self->async_send(std::move(msg));
            });
        }

        // Announce "stopped" on the strand when the worker exits.
        boost::asio::post(self->strand_, [self, gen]{
            if (gen == self->worker_gen_.load(std::memory_order_relaxed)) {
                self->worker_running_.store(false, std::memory_order_relaxed);
                // std::cout << "[broker] subscriber worker stopped (gen=" << gen << ")\n";
            }
        });
    });
}

void Broker::SubscriberSession::async_send(std::string msg) {
    if (stopped_) return ;

    struct WriteState {
        std::array<char, sizeof(uint32_t)> header;
        std::string payload;
    };

    auto state = std::make_shared<WriteState>();

    uint32_t len = static_cast<uint32_t>(msg.size());
    uint32_t netlen = htonl(len);

    std::memcpy(state->header.data(), &netlen, sizeof(uint32_t));
    state->payload = std::move(msg);

    std::array<boost::asio::const_buffer, 2> bufs{
        boost::asio::buffer(state->header),
        boost::asio::buffer(state->payload)
    };

    auto self = shared_from_this();
    boost::asio::async_write(socket_, bufs,
        boost::asio::bind_executor(strand_,
            [this, self, state](boost::system::error_code ec, std::size_t) {
                if (ec) {
                    stopped_ = true;
                    if (queue_) queue_->close();
                } else {
                    std::cout << "[broker] sent to subscriber socket\n";
                }
            }));
}

int main(int argc, char* argv[]) {
    std::cout << "[broker] main() starting\n";
    if (argc != 3) {
        std::cerr << "Usage: broker <pub_port> <sub_port>";
        return 1;
    }
    
    unsigned short pub_port = static_cast<unsigned short>(std::atoi(argv[1]));
    unsigned short sub_port = static_cast<unsigned short>(std::atoi(argv[2]));

    auto pick_kind = [](const char* v)->QueueKind{
        if (!v) return QueueKind::Mutex;
        std::string s{v};
        if (s == "lf")   return QueueKind::LockFree;
        if (s == "spsc") return QueueKind::Spsc;
        return QueueKind::Mutex;
    };
    QueueKind kind = pick_kind(std::getenv("BROKER_QUEUE"));
    boost::asio::io_context io_context;
    auto ctx = std::make_shared<BrokerContext>();
    Broker broker(io_context, pub_port, sub_port, kind, ctx);
    // Enter the I/O event loop, dispatching asynchronous handlers
    io_context.run();
    return 0;
}