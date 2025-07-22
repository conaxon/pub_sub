// filename: mutex_queue.hpp
#pragma once
#include <message_queue.hpp>
#include <queue>
#include <mutex>
#include <condition_variable>

// MutexQueue: blocking, uses condition_variable to avoid busy-waiting
// its reliable and easy to implement for refresher and no concurrency woes

template<typename T>
class MutexQueue: public MessageQueue<T> {
private:
    std::queue<T> queue_;
    std::mutex mutex_;
    std::condition_variable cond_;
    bool open_ = true;
public:
    void push(const T& item) override{
        {
            std::lock_guard<std::mutex> lock(mutex_);
            queue_.push(item);
        }
            cond_.notify_one();
    }

    std::optional<T> wait_and_pop() override {
        std::unique_lock<std::mutex> lock(mutex_);
        cond_.wait(lock, [this]{return !open_ || !queue_.empty();});
        if (!queue_.empty()) {
        T value = queue_.front();
        queue_.pop();
        return value;
        }

        return std::nullopt;
    }

    void close() override {
        {
        std::lock_guard<std::mutex> lock(mutex_);
        open_ = false;
        }
        cond_.notify_all();
    }
};