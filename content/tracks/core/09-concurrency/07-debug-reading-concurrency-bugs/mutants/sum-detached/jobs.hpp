// MUTANT: the thread is detached: the sum is read while the thread may still be writing it.
#pragma once

#include <atomic>
#include <condition_variable>
#include <exception>
#include <functional>
#include <mutex>
#include <stdexcept>
#include <thread>
#include <vector>

inline long long count_in_parallel(int threads, int per_thread) {
    std::atomic<long long> counter{0};
    std::vector<std::jthread> workers;
    for (int t = 0; t < threads; ++t) {
        workers.emplace_back([&counter, per_thread] {
            for (int i = 0; i < per_thread; ++i) counter.fetch_add(1, std::memory_order_relaxed);
        });
    }
    workers.clear();  // joins them all
    return counter.load();
}

class Pair {
public:
    void forward() {
        std::scoped_lock lock(mutex_a_, mutex_b_);  // both mutexes, in a safe order, whatever the order of the arguments
        --a_;
        ++b_;
    }
    void backward() {
        std::scoped_lock lock(mutex_a_, mutex_b_);
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
        cv_.wait(lock, [this] { return set_; });  // the predicate is checked first, so an event that already happened is not missed
    }

private:
    std::mutex mutex_;
    std::condition_variable cv_;
    bool set_ = false;
};

inline long long sum_in_background(const std::vector<int>& values) {
    long long result = 0;
    std::thread worker([&] {
        for (int v : values) result += v;
    });
    worker.detach();
    return result;
}

class Worker {
public:
    void start() {
        thread_ = std::thread([this] {
            while (!stop_.load()) {
                loops_.fetch_add(1, std::memory_order_relaxed);
                std::this_thread::yield();
            }
        });
    }
    void stop() {
        stop_.store(true);
        if (thread_.joinable()) thread_.join();
    }
    long long loops() const { return loops_.load(std::memory_order_relaxed); }

private:
    std::thread thread_;
    std::atomic<bool> stop_{false};
    std::atomic<long long> loops_{0};
};

inline void run_in_thread(const std::function<void()>& job) {
    std::exception_ptr error;
    std::thread t([&job, &error] {
        try {
            job();
        } catch (...) {
            error = std::current_exception();  // carry the exception over to the calling thread
        }
    });
    t.join();
    if (error) std::rethrow_exception(error);
}
