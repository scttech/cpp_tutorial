#pragma once

#include <condition_variable>
#include <cstddef>
#include <deque>
#include <functional>
#include <future>
#include <mutex>
#include <stdexcept>
#include <thread>
#include <type_traits>
#include <utility>
#include <vector>

// A thread pool: a fixed number of worker threads that take tasks from a queue. Starting a thread costs tens of microseconds and a thread costs
// memory: a program that has thousands of small jobs should reuse a few threads instead of starting one per job.
//
//   ThreadPool pool(n)             n worker threads (std::invalid_argument for n == 0), started at once
//   auto f = pool.submit(fn, args...)   queue fn(args...) for a worker; returns a std::future<result type>. The result, or the exception that the task
//                                  threw, arrives through the future. Arguments are copied/moved into the task (use std::ref for a reference).
//                                  After shutdown(): std::runtime_error.
//   pool.size()                    the number of worker threads
//   pool.shutdown()                accepts no more tasks, lets the workers FINISH every task already queued, and joins them. Safe to call twice.
//   ~ThreadPool()                  does what shutdown() does
//   parallel_map(pool, input, fn)  fn applied to every element on the pool, the results in the ORDER OF THE INPUT; the first failure is rethrown
//
// THE STARTER RUNS EVERY TASK ON THE CALLING THREAD, right inside submit(): it compiles and gives the right answers, and is not a pool at all.
class ThreadPool {
public:
    explicit ThreadPool(std::size_t threads) : size_(threads) {
        if (threads == 0) throw std::invalid_argument("a pool needs at least one thread");
    }
    ThreadPool(const ThreadPool&) = delete;
    ThreadPool& operator=(const ThreadPool&) = delete;
    ~ThreadPool() { shutdown(); }

    template <class F, class... Args>
    auto submit(F&& f, Args&&... args) -> std::future<std::invoke_result_t<F, Args...>> {
        using R = std::invoke_result_t<F, Args...>;
        // A packaged_task runs a function and puts its result (or its exception) into a future. It can be moved, not copied.
        std::packaged_task<R()> task(std::bind(std::forward<F>(f), std::forward<Args>(args)...));
        std::future<R> future = task.get_future();
        task();  // TODO: hand the task to a worker thread instead of running it here
        return future;
    }

    std::size_t size() const { return size_; }

    void shutdown() {}

private:
    std::size_t size_;
    // TODO: the queue of tasks, a mutex, a condition variable, a stop flag, and the worker threads.
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
