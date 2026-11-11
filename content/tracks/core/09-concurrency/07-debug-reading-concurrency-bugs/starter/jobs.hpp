#pragma once

#include <atomic>
#include <condition_variable>
#include <exception>
#include <functional>
#include <mutex>
#include <stdexcept>
#include <thread>
#include <vector>

// Six small concurrent tools. Each contains ONE concurrency bug, and each bug shows itself in a different way. Do not guess: run the demo, look at
// HOW the program stopped, read the message, and fix that one. Never fix the tests, and never "fix" a bug by adding a sleep.

// 1. count_in_parallel(threads, per_thread): `threads` threads each add 1 to one shared counter `per_thread` times. Returns the final value.
inline long long count_in_parallel(int threads, int per_thread) {
    long long counter = 0;
    std::vector<std::jthread> workers;
    for (int t = 0; t < threads; ++t) {
        workers.emplace_back([&counter, per_thread] {
            for (int i = 0; i < per_thread; ++i) ++counter;
        });
    }
    workers.clear();  // joins them all
    return counter;
}

// 2. Pair: two numbers, each with its own mutex. forward() moves 1 from a to b, backward() moves 1 from b to a; each needs BOTH mutexes.
//    a() and b() read one of them. The pair starts as a = 100, b = 0.
class Pair {
public:
    void forward() {
        std::lock_guard lock_a(mutex_a_);
        std::lock_guard lock_b(mutex_b_);
        --a_;
        ++b_;
    }
    void backward() {
        std::lock_guard lock_b(mutex_b_);
        std::lock_guard lock_a(mutex_a_);
        ++a_;
        --b_;
    }
    int a() const {
        std::scoped_lock lock(mutex_a_, mutex_b_);
        return a_;
    }
    int b() const {
        std::scoped_lock lock(mutex_a_, mutex_b_);
        return b_;
    }

private:
    mutable std::mutex mutex_a_;
    mutable std::mutex mutex_b_;
    int a_ = 100;
    int b_ = 0;
};

// 3. Event: set() marks it as happened, and wakes the threads that wait. wait() returns when the event has happened: AT ONCE if it already has.
class Event {
public:
    void set() {
        {
            std::lock_guard lock(mutex_);
            set_ = true;
        }
        cv_.notify_all();
    }
    void wait() {
        std::unique_lock lock(mutex_);
        cv_.wait(lock);
    }

private:
    std::mutex mutex_;
    std::condition_variable cv_;
    bool set_ = false;
};

// 4. sum_in_background(values): adds the values up on a BACKGROUND thread, and returns the sum. (When it returns, the thread has finished.)
inline long long sum_in_background(const std::vector<int>& values) {
    long long result = 0;
    std::thread worker([&] {
        for (int v : values) result += v;
    });
    return result;
}

// 5. Worker: start() launches a thread that counts loops until stop() is called; stop() ends it and waits for it. loops() says how many it has done
//    (it may be called while the worker runs).
class Worker {
public:
    void start() {
        thread_ = std::thread([this] {
            while (!stop_) {
                ++loops_;
                std::this_thread::yield();
            }
        });
    }
    void stop() {
        stop_ = true;
        thread_.join();
    }
    long long loops() const { return loops_; }

private:
    std::thread thread_;
    bool stop_ = false;
    long long loops_ = 0;
};

// 6. run_in_thread(job): runs `job` on a new thread and waits for it. If the job throws, the exception must come out of run_in_thread, in the CALLER.
inline void run_in_thread(const std::function<void()>& job) {
    std::thread t([&job] { job(); });
    t.join();
}
