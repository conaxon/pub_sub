#pragma once
#include <atomic>
#include <cstddef>
#include <type_traits>
#include <optional>
#include <thread>
#include <chrono>

// single-producer and single consumer (lock-free)
// capacity is power of two
// push returns false if full returns std::nullopt if empty
// for blocking behavior, caller can spin/yield or sleep

template<typename T, std::size_t Capacity>
class SpscRingBuffer {
    static_assert((Capacity & (Capacity - 1 )) == 0);
    static constexpr std::size_t MASK = Capacity - 1;

public:
    SpscRingBuffer(): head_(0), tail_(0) {}

    bool push(const T& v) {
        return emplace(v);
    }

    bool push(T&& v) {
        return emplace(std::move(v));
    }

    std::optional<T> pop() {
        const std::size_t head = head_.load(std::memory_order_acquire);
        if (head == tail_.load(std::memory_order_relaxed)) {
            return std::nullopt;
        }
        T value = std::move(buffer_[head & MASK]);
        head_.store(head + 1, std::memory_order_release);
        return value;
    }

    bool empty() const {
        return head_.load(std::memory_order_acquire) == tail_.load(std::memory_order_acquire);
    }
    bool full() const {
        return ((tail_.load(std::memory_order_acquire) - head_.load(std::memory_order_acquire)) == Capacity); 
    }
    std::size_t size() const {
        return tail_.load(std::memory_order_acquire) - head_.load(std::memory_order_acquire);
    }

private:
    template<typename U>
    bool emplace(U&& v) {
        const std::size_t tail = tail_.load(std::memory_order_relaxed);
        const std::size_t next_tail = tail + 1;
        if ((next_tail - head_.load(std::memory_order_acquire)) > Capacity) {
            return false;
        }
        buffer_[tail & MASK] = std::forward<U>(v);
        tail_.store(next_tail, std::memory_order_release);
        return true;
    }
    
    alignas(64) std::atomic<std::size_t> head_;
    alignas(64) std::atomic<std::size_t> tail_;
    alignas(64) T buffer_[Capacity];

};