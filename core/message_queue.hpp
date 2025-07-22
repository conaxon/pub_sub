// filename: message_queue.hpp
#pragma once
#include <optional>

// A generic interface for mesage queues
// Clear abstraction so we can swap between mutex and lock-free 
// without changing the broker logic

template<typename T>
class MessageQueue {
public:
    virtual void push(const T& item) = 0;
    virtual std::optional<T> wait_and_pop() = 0;
    virtual void close() = 0;
    virtual ~MessageQueue() = default;
};