#pragma once

#include <atomic>
#include <cstddef>
#include <optional>
#include <thread>
#include <utility>
#include <vector>

// Five small tools made of atomics. THE STARTER IS WRONG ON PURPOSE: it uses plain variables, or atomics with the weakest memory order, and
// ThreadSanitizer reports every one of them. Press Run, read the report, fix one tool at a time.

// 1. Counter: add() from any number of threads at once; value() returns the sum so far. No mutex: one atomic read-modify-write is enough
//    (fetch_add), and since nothing else depends on the counter's value, the cheapest memory order (relaxed) is correct here.
class Counter {
public:
    void add(long long n) { count_ += n; }
    long long value() const { return count_; }

private:
    long long count_ = 0;  // TODO: std::atomic<long long>
};

// 2. SpinLock: a lock that waits by looping. lock(), try_lock() and unlock() make it usable with std::lock_guard<SpinLock>. The data that it protects
//    is ordinary (non-atomic) data, so the lock itself must create the "happens-before": taking the lock is an ACQUIRE, giving it back is a RELEASE.
class SpinLock {
public:
    void lock() {
        while (flag_.test_and_set(std::memory_order_relaxed)) std::this_thread::yield();
    }
    bool try_lock() { return !flag_.test_and_set(std::memory_order_relaxed); }
    void unlock() { flag_.clear(std::memory_order_relaxed); }

private:
    std::atomic_flag flag_ = ATOMIC_FLAG_INIT;
};

// 3. OneShot<T>: a value that one thread sets (once) and others wait for. get() returns the value, WAITING until it has been set; ready() asks
//    without waiting. The value is an ordinary member, written before the flag and read after it: that needs RELEASE (in set) and ACQUIRE (in get).
//    And get() must not burn the processor while it waits: a std::atomic<bool> can sleep: flag.wait(false, order) and flag.notify_all().
template <class T>
class OneShot {
public:
    void set(T value) {
        value_ = std::move(value);
        ready_.store(true, std::memory_order_relaxed);
    }
    const T& get() const {
        while (!ready_.load(std::memory_order_relaxed)) std::this_thread::yield();
        return value_;
    }
    bool ready() const { return ready_.load(std::memory_order_relaxed); }

private:
    T value_{};
    std::atomic<bool> ready_{false};
};

// 4. fetch_double(a): ATOMICALLY double the int that `a` holds, and return the value it had BEFORE. `a` is a std::atomic<int> (the tests pass a wrapper
//    that has the same four functions: load, store, compare_exchange_weak, compare_exchange_strong). load() then store() is NOT atomic: another thread
//    can change the value in between. A compare_exchange loop retries until the value it read is still the value that it replaces.
template <class Atomic>
int fetch_double(Atomic& a) {
    int old = a.load();
    a.store(old * 2);
    return old;
}

// 5. LockFreeStack<T>: push() from any number of threads, with no mutex (a compare_exchange loop on the head). peek() copies the newest value (or nothing)
//    and may run while others push. drain() takes everything out, newest first; it is only called when no thread is pushing. Nodes are freed by drain()
//    and by the destructor. (There is no pop(): a safe lock-free pop needs a way to free nodes that other threads may still be reading, see the lesson.)
template <class T>
class LockFreeStack {
public:
    LockFreeStack() = default;
    LockFreeStack(const LockFreeStack&) = delete;
    LockFreeStack& operator=(const LockFreeStack&) = delete;
    ~LockFreeStack() { drain(); }

    void push(T value) {
        Node* node = new Node{std::move(value), head_};
        head_ = node;
    }

    std::optional<T> peek() const {
        Node* top = head_;
        if (top == nullptr) return std::nullopt;
        return top->value;
    }

    std::vector<T> drain() {
        std::vector<T> out;
        Node* node = head_;
        head_ = nullptr;
        while (node != nullptr) {
            out.push_back(std::move(node->value));
            Node* next = node->next;
            delete node;
            node = next;
        }
        return out;
    }

private:
    struct Node {
        T value;
        Node* next;
    };
    Node* head_ = nullptr;  // TODO: std::atomic<Node*>
};
