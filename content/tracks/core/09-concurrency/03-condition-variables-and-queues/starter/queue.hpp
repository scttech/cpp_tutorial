#pragma once

#include <cstddef>
#include <condition_variable>
#include <deque>
#include <mutex>
#include <optional>
#include <stdexcept>
#include <utility>

// A queue that threads can share: producers push, consumers pop, and each waits (without wasting the processor) when it has to.
//
//   BoundedQueue<T> q(capacity)    capacity 0: std::invalid_argument
//   q.push(value)                  WAITS while the queue is full. Returns true when the value was added; false (and the value is dropped) if the
//                                  queue has been closed, also when it was closed while push was waiting.
//   q.pop()                        WAITS while the queue is empty and open. Returns the oldest value; or std::nullopt, but ONLY when the queue is
//                                  closed and has no values left (values that were queued before close() are still delivered).
//   q.try_pop()                    never waits: the oldest value, or std::nullopt if there is none right now
//   q.close()                      no more pushes; wakes every thread that is waiting in push or pop
//   q.closed(), q.size(), q.capacity()
//
// THE STARTER IS NOT A BLOCKING QUEUE: it is unbounded, it never waits and it is not thread-safe. Add a mutex and condition variables.
template <class T>
class BoundedQueue {
public:
    explicit BoundedQueue(std::size_t capacity) : capacity_(capacity) {
        if (capacity == 0) throw std::invalid_argument("capacity must be positive");
    }

    bool push(T value) {
        if (closed_) return false;
        items_.push_back(std::move(value));
        return true;
    }

    std::optional<T> pop() { return try_pop(); }

    std::optional<T> try_pop() {
        if (items_.empty()) return std::nullopt;
        T value = std::move(items_.front());
        items_.pop_front();
        return value;
    }

    void close() { closed_ = true; }
    bool closed() const { return closed_; }
    std::size_t size() const { return items_.size(); }
    std::size_t capacity() const { return capacity_; }

private:
    std::size_t capacity_;
    std::deque<T> items_;
    bool closed_ = false;
    // TODO: a mutex (mutable, so that the const accessors can lock it) and two condition variables: one for "not full", one for "not empty".
};
