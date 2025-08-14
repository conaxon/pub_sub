// filename: spsc_queue_adapter.hpp
#pragma once
#include <message_queue.hpp>
#include <optional>
#include <thread>
#include <chrono>
#include "spsc_ring_buffer.hpp"

// block-free wait using spin + sleep(0) ok for demo
// one producer and consumer

template<typename T, std::size_t CapacityPow2>
class SpscAdapter : public MessageQueue<T> {
public:
    void push(const T& item) override {
        while (!rb_.push(item)) {
            std::this_thread::yield();
        }
    }

    std::optional<T> wait_and_pop() override {
        for (;;) {
            auto v = rb_.pop();
            if (v) return v;
            if (!open_) return std::nullopt;
            std::this_thread::sleep_for(std::chrono::microseconds(50));
        }
    }

    void close() override {
        open_ = false;
    }

private:
    SpscRingBuffer<T,CapacityPow2> rb_;
    std::atomic<bool> open_{true};
};