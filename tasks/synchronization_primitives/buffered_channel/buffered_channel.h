#pragma once

#include <condition_variable>
#include <mutex>
#include <optional>
#include <queue>
#include <stdexcept>

template <class T>
class BufferedChannel {
public:
    explicit BufferedChannel(int size) : capacity(size) {
    }

    void Send(const T& value) {
        std::unique_lock lock{mtx};

        not_full.wait(lock, [this] {
            return queue.size() < static_cast<size_t>(capacity) || closed;
        });

        if (closed) {
            throw std::runtime_error("Channel is closed");
        }
        
        queue.push(value);
        not_empty.notify_one();
    }

    std::optional<T> Recv() {
        std::unique_lock lock{mtx};

        not_empty.wait(lock, [this] {
            return !queue.empty() || closed;
        });

        if (queue.empty()) {
            return std::nullopt;
        }
        
        T value = std::move(queue.front());
        queue.pop();
        not_full.notify_one();
        return value;
    }

    void Close() {
        std::unique_lock lock{mtx};
        closed = true;
        not_full.notify_all();
        not_empty.notify_all();
    }

private:
    int capacity;
    bool closed{false};
    std::mutex mtx;
    std::queue<T> queue;
    std::condition_variable not_full;
    std::condition_variable not_empty;
};