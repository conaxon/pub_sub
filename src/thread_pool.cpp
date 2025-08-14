// filename: src/thread_pool.cpp
#include "core/thread_pool.hpp"
#include <exception>
#include <iostream>

ThreadPool::ThreadPool(std::size_t n) {
    if (n == 0) n = 1;
    threads_.reserve(n);
    for (std::size_t i = 0; i < n; ++i) {
        threads_.emplace_back([this] { worker_loop(); });
    }
}

void ThreadPool::worker_loop() {
    for (;;) {
        std::function<void()> task;
        {
            std::unique_lock<std::mutex> lk(mu_);
            cv_.wait(lk, [this]{
                return stop_.load(std::memory_order_acquire) || !tasks_.empty();
            });
            if (stop_.load(std::memory_order_relaxed) && tasks_.empty())
                return; // graceful shutdown after draining
            task = std::move(tasks_.front());
            tasks_.pop();
        }
        try {
            task();
        } catch (const std::exception& e) {
            // keep the pool alive if a task throws
            std::cerr << "[thread_pool] task threw std::exception: " << e.what() << "\n";
        } catch (...) {
            std::cerr << "[thread_pool] task threw unknown exception\n";
        }
    }
}

ThreadPool::~ThreadPool() {
    stop_.store(true, std::memory_order_release);
    cv_.notify_all();
    for (auto& t : threads_) {
        if (t.joinable()) t.join();
    }
}
