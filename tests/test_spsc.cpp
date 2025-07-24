#include <gtest/gtest.h>
#include <thread>
#include <vector>
#include "core/spsc_ring_buffer.hpp"

TEST(SPSC, BasicPushPop) {
    SpscRingBuffer<int, 1024> rb;
    EXPECT_TRUE(rb.push(1));
    auto v = rb.pop();
    ASSERT_TRUE(v.has_value());
    EXPECT_EQ(*v,1);
}

TEST(SPSC, MultiItem) {
    SpscRingBuffer<int, 1024> rb;
    for (int i=0;i<100;i++) EXPECT_TRUE(rb.push(i));
    for (int i=0;i<100;i++) {
        auto v = rb.pop();
        ASSERT_TRUE(v.has_value());
        EXPECT_EQ(*v, i);
    }
}

TEST(SPSC, Threads) {
    constexpr int N = 100000;
    SpscRingBuffer<int, 1<<12> rb;
    std::thread prod([&]{ for(int i=0;i<N;i++){ while(!rb.push(i)) std::this_thread::yield(); } });
    std::thread cons([&]{ for(int i=0;i<N;i++){ std::optional<int> v; do { v = rb.pop(); if(!v) std::this_thread::yield(); } while(!v); EXPECT_EQ(*v, i); } });
    prod.join();
    cons.join();
}