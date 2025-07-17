#include <gtest/gtest.h>
#include "core/mutex_queue.hpp"
#include <thread>
#include <chrono>

TEST(MessageQueueTest, PushAndPop) {
    MuteQueue<int> q;
    std::thread producer([&q]() {
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
        q.push(42);
    });
}