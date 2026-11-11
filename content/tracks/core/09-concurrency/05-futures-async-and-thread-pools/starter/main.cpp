// A demo of the ThreadPool (read-only). Press Run. ThreadSanitizer is on.
#include <future>
#include <iostream>
#include <string>
#include <thread>
#include <vector>

#include "pool.hpp"

int main() {
    ThreadPool pool(3);

    // submit() returns a future: a ticket for a result that is not ready yet
    std::future<int> answer = pool.submit([] { return 6 * 7; });
    std::future<std::string> greeting = pool.submit([](std::string who) { return "hello, " + who; }, std::string("pool"));
    std::cout << "answer: " << answer.get() << ", greeting: " << greeting.get() << "\n";

    // an exception thrown by a task is delivered to whoever calls get()
    auto failing = pool.submit([]() -> int { throw std::runtime_error("the task failed"); });
    try {
        failing.get();
    } catch (const std::exception& e) {
        std::cout << "caught: " << e.what() << "\n";
    }

    // a whole vector at once
    const std::vector<int> numbers = {1, 2, 3, 4, 5, 6, 7, 8};
    const std::vector<int> squares = parallel_map(pool, numbers, [](int x) { return x * x; });
    std::cout << "squares:";
    for (int s : squares) std::cout << ' ' << s;
    std::cout << "\n";
    std::cout << "the pool has " << pool.size() << " worker threads\n";
}
