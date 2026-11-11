// MUTANT: the worker waits for any wake-up and does not check the queue (Clean tier, and a lost wake-up).
#pragma once

#include <condition_variable>
#include <cstddef>
#include <deque>
#include <functional>
#include <future>
#include <memory>
#include <mutex>
#include <stdexcept>
#include <thread>
#include <type_traits>
#include <utility>
#include <vector>

class ThreadPool {
public:
    explicit ThreadPool(std::size_t threads) : size_(threads) {
        if (threads == 0) throw std::invalid_argument("a pool needs at least one thread");
        workers_.reserve(threads);
        for (std::size_t i = 0; i < threads; ++i) workers_.emplace_back([this] { work(); });
    }
    ThreadPool(const ThreadPool&) = delete;
    ThreadPool& operator=(const ThreadPool&) = delete;
    ~ThreadPool() { shutdown(); }

    template <class F, class... Args>
    auto submit(F&& f, Args&&... args) -> std::future<std::invoke_result_t<F, Args...>> {
        using R = std::invoke_result_t<F, Args...>;
        // std::function needs a copyable callable, a packaged_task can only be moved: share it through a shared_ptr.
        auto task = std::make_shared<std::packaged_task<R()>>(std::bind(std::forward<F>(f), std::forward<Args>(args)...));
        std::future<R> future = task->get_future();
        {
            std::lock_guard lock(mutex_);
            if (stopping_) throw std::runtime_error("the pool has been shut down");
            queue_.push_back([task] { (*task)(); });
        }
        wake_.notify_one();
        return future;
    }

    std::size_t size() const { return size_; }

    void shutdown() {
        {
            std::lock_guard lock(mutex_);
            stopping_ = true;
        }
        wake_.notify_all();
        for (std::jthread& worker : workers_) {
            if (worker.joinable()) worker.join();
        }
    }

private:
    void work() {
        for (;;) {
            std::function<void()> job;
            {
                std::unique_lock lock(mutex_);
                wake_.wait(lock);
                if (queue_.empty()) return;  // stopping, and every queued task has been taken: done
                job = std::move(queue_.front());
                queue_.pop_front();
            }
            job();  // the packaged_task catches an exception and stores it in the future, so the worker survives
        }
    }

    const std::size_t size_;
    std::mutex mutex_;  // guards queue_ and stopping_
    std::condition_variable wake_;
    std::deque<std::function<void()>> queue_;
    bool stopping_ = false;
    std::vector<std::jthread> workers_;  // declared last: destroyed first
};

template <class T, class F>
auto parallel_map(ThreadPool& pool, const std::vector<T>& input, F fn) -> std::vector<std::invoke_result_t<F&, const T&>> {
    using R = std::invoke_result_t<F&, const T&>;
    std::vector<std::future<R>> futures;
    futures.reserve(input.size());
    for (const T& item : input) futures.push_back(pool.submit(fn, std::cref(item)));
    std::vector<R> out;
    out.reserve(input.size());
    for (std::future<R>& future : futures) out.push_back(future.get());
    return out;
}
