// filename: test_lf_queue.cpp
#include <gtest/gtest.h>
#include <thread>
#include <vector>
#include "core/lf_queue.hpp"

TEST(LfQueue, BasicPushPopLfQ) {
    LfQueue<int, 1024> q;
    q.push(1);
    auto v = q.wait_and_pop();
    ASSERT_TRUE(v.has_value());
    EXPECT_EQ(*v,1);
}

TEST(LfQueue, MultiItemLfQ) {
    LfQueue<int, 1024> q;
    for (int i = 0; i < 100; ++i) {q.push(i);}
    for (int i = 0; i < 100; ++i) {
        auto v = q.wait_and_pop();
        ASSERT_TRUE(v.has_value());
        EXPECT_EQ(*v, i);
    }
}

TEST(LfQueue, ThreadsLfQ) {
    constexpr int N = 100000;
    LfQueue<int, 1<<12> q;
    std::vector<int> results;
    results.reserve(N);

    std::thread prod([&] {
        for (int i = 0; i < N; ++i) {
            q.push(i);
        }
    });

    std::thread cons([&] {
        for (int i = 0; i < N; ++i) {
            auto v = q.wait_and_pop();
            results.push_back(*v);
        }
    });

    prod.join();
    cons.join();

    ASSERT_EQ(results.size(), N);
    for (int i = 0; i < N; ++i) {
        EXPECT_EQ(results[i], i);
    }
}