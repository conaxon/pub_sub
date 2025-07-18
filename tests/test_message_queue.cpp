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
    auto result = q.wait_and_pop();
    EXPECT_TRUE(result.has_value());
    EXPECT_EQ(result.value(), 42);
    producer.join();
}