#include <queue_factory.hpp>

std::shared_ptr<MessageQueue<std::string>> make_queue(QueueKind kind) {
    switch (kind) {
        case QueueKind::LockFree:
            return std::make_shared<LfQueue<std::string, DEFAULT_Q_CAP>>();
        case QueueKind::Spsc:
            return std::make_shared<SpscAdapter<std::string, DEFAULT_Q_CAP>>();
        default:
            return std::make_shared<MutexQueue<std::string>>();
    }
}
