#pragma once
#include <atomic>
#include <condition_variable>
#include <cstddef>
#include <functional>
#include <mutex>
#include <queue>
#include <thread>
#include <vector>

class ThreadPool {
public:
    explicit ThreadPool(std::size_t n);
    ~ThreadPool();

    ThreadPool(const ThreadPool&) = delete;
    ThreadPool& operator=(const ThreadPool&) = delete;

    template <class F>
    void post(F&& f) {
        if (stop_.load(std::memory_order_acquire)) return;   // ignore after stop
        {
            std::lock_guard<std::mutex> lk(mu_);
            if (stop_) return;
            tasks_.emplace(std::forward<F>(f));
        }
        cv_.notify_one();
    }

    std::size_t size() const noexcept { return threads_.size(); }

private:
    void worker_loop();

    std::atomic<bool> stop_{false};               
    std::mutex mu_;
    std::condition_variable cv_;
    std::queue<std::function<void()>> tasks_;
    std::vector<std::thread> threads_;
};
