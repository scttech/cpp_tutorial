// MUTANT: taking the lock is not an acquire: the data it protects is not ordered after the previous owner.
#pragma once

#include <atomic>
#include <cstddef>
#include <optional>
#include <thread>
#include <utility>
#include <vector>

class Counter {
public:
    void add(long long n) { count_.fetch_add(n, std::memory_order_relaxed); }  // relaxed: nothing else depends on the order of these additions
    long long value() const { return count_.load(std::memory_order_relaxed); }

private:
    std::atomic<long long> count_{0};
};

class SpinLock {
public:
    void lock() {
        while (flag_.test_and_set(std::memory_order_relaxed)) std::this_thread::yield();
    }
    bool try_lock() { return !flag_.test_and_set(std::memory_order_acquire); }
    void unlock() { flag_.clear(std::memory_order_release); }  // release: what we did is visible to the next owner

private:
    std::atomic_flag flag_ = ATOMIC_FLAG_INIT;
};

template <class T>
class OneShot {
public:
    void set(T value) {
        value_ = std::move(value);
        ready_.store(true, std::memory_order_release);  // publishes value_
        ready_.notify_all();                            // wakes the threads that sleep in get()
    }
    const T& get() const {
        ready_.wait(false, std::memory_order_acquire);  // sleeps while the flag is false; the acquire pairs with the release in set()
        return value_;
    }
    bool ready() const { return ready_.load(std::memory_order_acquire); }

private:
    T value_{};
    std::atomic<bool> ready_{false};
};

template <class Atomic>
int fetch_double(Atomic& a) {
    int old = a.load();
    while (!a.compare_exchange_weak(old, old * 2)) {
        // the exchange failed: `old` now holds the value that is really there, so the next try doubles THAT
    }
    return old;
}

template <class T>
class LockFreeStack {
public:
    LockFreeStack() = default;
    LockFreeStack(const LockFreeStack&) = delete;
    LockFreeStack& operator=(const LockFreeStack&) = delete;
    ~LockFreeStack() { drain(); }

    void push(T value) {
        Node* node = new Node{std::move(value), head_.load(std::memory_order_relaxed)};
        // Publish the node: release, so that a thread that sees the new head also sees node->value and node->next. On failure `node->next` is
        // refreshed with the head that is really there, and we try again.
        while (!head_.compare_exchange_weak(node->next, node, std::memory_order_release, std::memory_order_relaxed)) {
        }
    }

    std::optional<T> peek() const {
        Node* top = head_.load(std::memory_order_acquire);
        if (top == nullptr) return std::nullopt;
        return top->value;
    }

    std::vector<T> drain() {
        std::vector<T> out;
        Node* node = head_.exchange(nullptr, std::memory_order_acquire);
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
    std::atomic<Node*> head_{nullptr};
};
