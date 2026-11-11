// A demo of workers.hpp (read-only). Press Run. ThreadSanitizer is on: a data race in your code stops the program with a report.
#include <atomic>
#include <chrono>
#include <iostream>
#include <thread>
#include <vector>

#include "workers.hpp"

int main() {
    std::vector<int> numbers;
    fill_in_thread(numbers, 5);
    std::cout << "filled by a thread:";
    for (int x : numbers) std::cout << ' ' << x;
    std::cout << "\n";

    std::atomic<int> done{0};
    std::vector<std::function<void()>> jobs;
    for (int i = 0; i < 3; ++i) jobs.push_back([&done] { ++done; });
    jobs.push_back([] { throw std::runtime_error("job 3 failed"); });
    try {
        run_all(jobs);
    } catch (const std::exception& e) {
        std::cout << "run_all: " << e.what() << ", but " << done.load() << " other jobs finished\n";
    }

    std::atomic<int> ticks{0};
    {
        Ticker ticker(std::chrono::milliseconds(10), [&ticks] { ++ticks; });
        std::this_thread::sleep_for(std::chrono::milliseconds(60));
    }
    std::cout << "the ticker ticked at least 3 times: " << (ticks.load() >= 3 ? "yes" : "no") << "\n";
    std::cout << "hardware threads: " << std::thread::hardware_concurrency() << "\n";
}
