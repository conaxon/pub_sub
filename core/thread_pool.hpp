// core/thread_pool.hpp
#pragma once
#include <queue>
#include <mutex>
#include <condition_variable>
#include <thread>
#include <vector>
#include <functional>
#include <atomic>

class ThreadPool {
public:
    explicit ThreadPool(size_t n = std::thread::hardware_concurrency())
        : stop_(false) {
        workers_.reserve(n);
        for (size_t i = 0; i < n; ++i) {
            workers_.emplace_back([this] { worker(); });
        }
    }
    ~ThreadPool() {
        {
            std::lock_guard<std::mutex> lk(m_);
            stop_ = true;
        }
        cv_.notify_all();
        for (auto &t : workers_) t.join();
    }

    void post(std::function<void()> fn) {
        {
            std::lock_guard<std::mutex> lk(m_);
            tasks_.push(std::move(fn));
        }
        cv_.notify_one();
    }

private:
    void worker() {
        for (;;) {
            std::function<void()> fn;
            {
                std::unique_lock<std::mutex> lk(m_);
                cv_.wait(lk, [this]{ return stop_ || !tasks_.empty(); });
                if (stop_ && tasks_.empty()) return;
                fn = std::move(tasks_.front());
                tasks_.pop();
            }
            fn();
        }
    }

    std::queue<std::function<void()>> tasks_;
    std::mutex m_;
    std::condition_variable cv_;
    std::vector<std::thread> workers_;
    bool stop_;
};