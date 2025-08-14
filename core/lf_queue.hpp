// filename: core/lf_queue.hpp
#pragma once
#include <boost/lockfree/queue.hpp>
#include <message_queue.hpp>
#include <optional>
#include <thread>
#include <chrono>
#include <new>

// MPMC lock‑free queue adapter: allocates each T on the heap.
// Template parameter must be a power of two.
template<typename T, std::size_t CapacityPow2 = 1024>
class LfQueue : public MessageQueue<T> {
public:
    LfQueue()
      : q_(CapacityPow2)
      , open_(true)
    {}

    /*
    void push(const T& item) override {
        // allocate a copy on the heap
        T* p = new T(item);
        // spin until enqueued
        while (!q_.push(p)) {
            std::this_thread::yield();
        }
    }
    */

    bool try_push(const T& item) override {
        if (!open_.load(std::memory_order_relaxed)) return false;
        T* p = new (std::nothrow) T(item);
        if (!p) return false;
        if (q_.push(p)) return true;
        delete p;
        return false;
    }

    void push(const T& item) override {
        while (!try_push(item)) {
            if (!open_.load(std::memory_order_relaxed)) return;
            std::this_thread::yield();
        }
    }

    std::optional<T> wait_and_pop() override {
        T* p = nullptr;
        while (true) {
            if (q_.pop(p)) {
                // got one
                T value = std::move(*p);
                delete p;
                return value;
            }
            if (!open_) {
                return std::nullopt;
            }
            std::this_thread::sleep_for(std::chrono::microseconds(50));
        }
    }

    void close() override {
        open_.store(false, std::memory_order_relaxed);
    }

    ~LfQueue() {
        T* p = nullptr;
        while (q_.pop(p)) { delete p; }
    }

private:
    boost::lockfree::queue<T*> q_;
    std::atomic<bool>   open_;
};