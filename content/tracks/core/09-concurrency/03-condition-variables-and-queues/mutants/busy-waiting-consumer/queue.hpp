// MUTANT: pop spins (unlock, yield, lock) instead of waiting on the condition variable: it works, and burns the processor.
#pragma once

#include <cstddef>
#include <condition_variable>
#include <deque>
#include <mutex>
#include <optional>
#include <stdexcept>
#include <thread>
#include <utility>

template <class T>
class BoundedQueue {
public:
    explicit BoundedQueue(std::size_t capacity) : capacity_(capacity) {
        if (capacity == 0) throw std::invalid_argument("capacity must be positive");
    }

    bool push(T value) {
        std::unique_lock lock(mutex_);
        // Always wait with a PREDICATE: it is re-checked after every wake-up (spurious ones included), and checked first, so a change that
        // happened before we started waiting is not missed.
        not_full_.wait(lock, [this] { return closed_ || items_.size() < capacity_; });
        if (closed_) return false;
        items_.push_back(std::move(value));
        lock.unlock();  // do not wake a thread that would immediately block on our mutex
        not_empty_.notify_one();
        return true;
    }

    std::optional<T> pop() {
        std::unique_lock lock(mutex_);
        while (!closed_ && items_.empty()) {
            lock.unlock();
            std::this_thread::yield();
            lock.lock();
        }
        if (items_.empty()) return std::nullopt;
        T value = std::move(items_.front());
        items_.pop_front();
        lock.unlock();
        not_full_.notify_one();
        return value;
    }

    std::optional<T> try_pop() {
        std::unique_lock lock(mutex_);
        if (items_.empty()) return std::nullopt;
        T value = std::move(items_.front());
        items_.pop_front();
        lock.unlock();
        not_full_.notify_one();
        return value;
    }

    void close() {
        {
            std::lock_guard lock(mutex_);
            closed_ = true;
        }
        not_full_.notify_all();   // every waiting producer must see that the queue is closed...
        not_empty_.notify_all();  // ...and so must every waiting consumer
    }

    bool closed() const {
        std::lock_guard lock(mutex_);
        return closed_;
    }

    std::size_t size() const {
        std::lock_guard lock(mutex_);
        return items_.size();
    }

    std::size_t capacity() const { return capacity_; }

private:
    const std::size_t capacity_;
    mutable std::mutex mutex_;  // guards items_ and closed_
    std::condition_variable not_full_;
    std::condition_variable not_empty_;
    std::deque<T> items_;
    bool closed_ = false;
};
