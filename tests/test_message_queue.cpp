//filename: tests/test_message_queue.cpp
#include <gtest/gtest.h>
#include "core/mutex_queue.hpp"
#include <thread>
#include <chrono>

TEST(MessageQueueTest, PushAndPop) {
    MutexQueue<int> q;
    std::thread producer([&q]() {
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
        q.push(42);
    });

    auto start = std::chrono::steady_clock::now();
    std::optional<int> result;
    while (std::chrono::steady_clock::now() - start < std::chrono::seconds(1)) {
        result = q.try_pop();
        if (result.has_value()) break;
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    EXPECT_TRUE(result.has_value());
    EXPECT_EQ(result.value(), 42);
    producer.join();
}