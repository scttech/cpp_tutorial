// MUTANT: the first tick comes after one period instead of at once.
#pragma once

#include <chrono>
#include <condition_variable>
#include <exception>
#include <functional>
#include <mutex>
#include <stdexcept>
#include <stop_token>
#include <thread>
#include <vector>

namespace detail {
inline void fill_worker(std::vector<int>& out, int n) {
    out.clear();
    for (int i = 0; i < n; ++i) out.push_back(i);
}
}  // namespace detail

inline void fill_in_thread(std::vector<int>& out, int n) {
    std::thread t(detail::fill_worker, std::ref(out), n);  // std::ref: the thread gets the real vector, not a copy
    t.join();
}

inline void run_all(std::vector<std::function<void()>> jobs) {
    std::vector<std::exception_ptr> errors(jobs.size());
    {
        std::vector<std::jthread> threads;
        threads.reserve(jobs.size());
        for (std::size_t i = 0; i < jobs.size(); ++i) {
            threads.emplace_back([&jobs, &errors, i] {
                try {
                    jobs[i]();
                } catch (...) {
                    errors[i] = std::current_exception();  // each thread writes only its own slot
                }
            });
        }
    }  // all the jthreads are joined here
    for (const std::exception_ptr& error : errors) {
        if (error) std::rethrow_exception(error);
    }
}

class Ticker {
public:
    Ticker(std::chrono::milliseconds period, std::function<void()> on_tick)
        : thread_([this, period, tick = std::move(on_tick)](std::stop_token stop) {
              std::unique_lock lock(mutex_);
              while (!stop.stop_requested()) {
                  wake_.wait_for(lock, stop, period, [] { return false; });
                  if (stop.stop_requested()) break;
                  lock.unlock();
                  tick();
                  lock.lock();
              }
          }) {}

    Ticker(const Ticker&) = delete;
    Ticker& operator=(const Ticker&) = delete;

private:
    std::mutex mutex_;
    std::condition_variable_any wake_;
    std::jthread thread_;  // declared LAST: destroyed FIRST, so the thread is stopped and joined while the mutex and the condition variable still exist
};
