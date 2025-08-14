// filename: core/broker_context.hpp
#pragma once
#include "core/metrics.hpp"
#include <unordered_map>
#include <mutex>
#include <chrono>
#include <string>

struct BanEntry {
    std::chrono::steady_clock::time_point until{};
    int strikes{0};
};

struct BrokerContext {
    Metrics metrics;

    std::mutex bad_peers_mu;
    std::unordered_map<std::string, BanEntry> bad_peers;

    int bad_frame_threshold = 3;
    std::chrono::minutes ban_duration{std::chrono::minutes(5)};
};
