// filename: core/metrics.hpp
#pragma once
#include <atomic>
#include <chrono>

struct Metrics {
    std::atomic<uint16_t> published{0};
    std::atomic<uint16_t> delivered{0};
    std::atomic<uint64_t> malformed_frames{0};
};

inline uint64_t now_ns() {
    return std::chrono::duration_cast<std::chrono::nanoseconds>(
               std::chrono::steady_clock::now().time_since_epoch())
        .count();
}
