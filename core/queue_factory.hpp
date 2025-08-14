// filename: queue_factory.hpp
#pragma once
#include <memory>
#include <string>
#include <message_queue.hpp>
#include <mutex_queue.hpp>
#include <lf_queue.hpp>
#include <spsc_queue_adapter.hpp>

enum class QueueKind {Mutex, LockFree, Spsc};

constexpr std::size_t DEFAULT_Q_CAP = 1 <<12;

std::shared_ptr<MessageQueue<std::string>> make_queue(QueueKind kind);