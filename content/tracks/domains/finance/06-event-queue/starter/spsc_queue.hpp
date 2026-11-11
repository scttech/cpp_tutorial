#pragma once
// YOUR SPSC QUEUE: a bounded FIFO between exactly ONE producer thread and ONE consumer thread, with no mutex.
//
//   SpscQueue<T, N>        holds up to N items (N >= 1)
//   try_push(item)         producer thread only. false if the queue is full (the item is left unchanged for lvalues / not moved from)
//   try_pop(out)           consumer thread only. false if the queue is empty; otherwise moves the oldest item into `out`
//   size_approx()          callable from any thread; 0..N; exact when no other thread is working
//   empty()                the same (approximate from another thread)
//   capacity()             N
//
// How: a ring buffer of N + 1 slots (one stays free so that "full" and "empty" differ), a `tail` index that only the producer
// writes and a `head` index that only the consumer writes, both std::atomic. The producer writes the slot FIRST and then publishes
// it with a store(release) of the new tail; the consumer reads the tail with load(acquire) before it touches the slot. Each thread
// reads its OWN index with memory_order_relaxed. Put head and tail on different cache lines (alignas(64)): otherwise the two
// threads keep stealing the line from each other (false sharing). Items may be move-only. No mutex, no allocation.
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
        (void)item;
        return false;  // TODO
    }
    bool try_pop(T& out) {
        (void)out;
        return false;  // TODO
    }
    std::size_t size_approx() const { return 0; }  // TODO
    bool empty() const { return true; }            // TODO
    static constexpr std::size_t capacity() { return N; }

private:
    // TODO: std::array<T, N + 1> buffer_; the two atomic indices on separate cache lines
};

}  // namespace evq
