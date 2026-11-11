// MUTANT: size_approx is wrong when the indices wrapped
#pragma once
// A ring buffer of N + 1 slots and two atomic indices, each written by one thread only; release/acquire publishes the slots.
#include <array>
#include <atomic>
#include <cstddef>
#include <utility>

namespace evq {

template <class T, std::size_t N>
class SpscQueue {
    static_assert(N >= 1, "capacity must be at least 1");

public:
    bool try_push(T item) {
        const std::size_t tail = tail_.load(std::memory_order_relaxed);  // only this thread writes it
        const std::size_t next = advance(tail);
        if (next == head_.load(std::memory_order_acquire)) return false;  // full: the consumer has not freed a slot
        buffer_[tail] = std::move(item);                                  // write the slot ...
        tail_.store(next, std::memory_order_release);                     // ... then publish it
        return true;
    }

    bool try_pop(T& out) {
        const std::size_t head = head_.load(std::memory_order_relaxed);  // only this thread writes it
        if (head == tail_.load(std::memory_order_acquire)) return false; // empty
        out = std::move(buffer_[head]);                                   // read the slot ...
        head_.store(advance(head), std::memory_order_release);           // ... then give it back
        return true;
    }

    std::size_t size_approx() const {
        const std::size_t head = head_.load(std::memory_order_acquire);
        const std::size_t tail = tail_.load(std::memory_order_acquire);
        return tail >= head ? tail - head : tail + N - head;
    }
    bool empty() const { return head_.load(std::memory_order_acquire) == tail_.load(std::memory_order_acquire); }
    static constexpr std::size_t capacity() { return N; }

private:
    static std::size_t advance(std::size_t i) { return i + 1 == N + 1 ? 0 : i + 1; }

    alignas(64) std::atomic<std::size_t> head_{0};  // next slot to read: written by the consumer
    alignas(64) std::atomic<std::size_t> tail_{0};  // next slot to write: written by the producer
    alignas(64) std::array<T, N + 1> buffer_{};
};

}  // namespace evq
