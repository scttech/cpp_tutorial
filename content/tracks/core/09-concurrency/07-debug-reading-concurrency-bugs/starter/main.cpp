// The demo of jobs.hpp (read-only). It runs six scenarios, one after the other, and prints a line BEFORE each one. Press Run: the program stops at the
// first scenario that is broken, and HOW it stops tells you the kind of bug. Fix it in jobs.hpp, press Run again, and meet the next one.
#include <iostream>
#include <thread>
#include <vector>

#include "jobs.hpp"

int main() {
    std::cout << "1. four threads count to 1000 each ..." << std::endl;
    std::cout << "   total " << count_in_parallel(4, 1000) << " (expected 4000)" << std::endl;

    std::cout << "2. two threads move money between two accounts, one after the other ..." << std::endl;
    Pair pair;
    std::jthread forward([&] { for (int i = 0; i < 10; ++i) pair.forward(); });
    forward.join();
    std::jthread backward([&] { for (int i = 0; i < 4; ++i) pair.backward(); });
    backward.join();
    std::cout << "   a = " << pair.a() << ", b = " << pair.b() << " (expected 94 and 6)" << std::endl;

    std::cout << "3. an event that is set BEFORE somebody waits for it ..." << std::endl;
    Event event;
    event.set();
    std::jthread waiter([&] { event.wait(); });
    waiter.join();
    std::cout << "   the waiter came back" << std::endl;

    std::cout << "4. a sum computed in the background ..." << std::endl;
    std::cout << "   sum " << sum_in_background({1, 2, 3, 4, 5}) << " (expected 15)" << std::endl;

    std::cout << "5. a worker that loops until it is told to stop ..." << std::endl;
    Worker worker;
    worker.start();
    while (worker.loops() < 100) std::this_thread::yield();
    worker.stop();
    std::cout << "   the worker stopped" << std::endl;

    std::cout << "6. a job that throws, run on a thread, with the exception caught by the caller ..." << std::endl;
    try {
        run_in_thread([] { throw std::runtime_error("the job failed"); });
    } catch (const std::exception& e) {
        std::cout << "   caught: " << e.what() << std::endl;
    }
    std::cout << "all six scenarios ran" << std::endl;
}
