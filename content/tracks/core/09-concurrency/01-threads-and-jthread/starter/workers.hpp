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

// Three small tools made of threads. Run the demo (main.cpp) and Run Tests as you go.

// 1. fill_in_thread(out, n): start a NEW thread that replaces the contents of `out` with 0, 1, ..., n-1, and return when that thread is finished.
//    (std::thread COPIES its arguments. To give a thread a reference to your variable, wrap it: std::ref(out), or capture it in a lambda.)
inline void fill_in_thread(std::vector<int>& out, int n) {
    (void)out;
    (void)n;
}

// 2. run_all(jobs): run every job on a thread of its own, ALL AT THE SAME TIME (a job may wait for another job), and return when all of them are
//    finished. A job that throws must not take the program down: the other jobs still run to the end, and afterwards run_all rethrows the exception of
//    the job with the LOWEST index that failed (std::exception_ptr, std::current_exception, std::rethrow_exception).
inline void run_all(std::vector<std::function<void()>> jobs) {
    for (auto& job : jobs) job();  // one after the other: the demo works, but the tests with jobs that wait for each other do not
}

// 3. Ticker(period, on_tick): a thread that calls on_tick() at once and then every `period`, until the Ticker is destroyed. The destructor must stop
//    the thread PROMPTLY, even when the period is ten seconds (sleep_for cannot be interrupted: wait on a condition variable with the stop token),
//    and no tick may happen after the destructor has returned. Use std::jthread: it joins by itself and hands the thread function a std::stop_token.
class Ticker {
public:
    Ticker(std::chrono::milliseconds period, std::function<void()> on_tick) {
        (void)period;
        (void)on_tick;
    }
    Ticker(const Ticker&) = delete;
    Ticker& operator=(const Ticker&) = delete;
};
