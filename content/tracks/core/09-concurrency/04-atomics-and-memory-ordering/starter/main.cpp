// A demo of atomics.hpp (read-only). Press Run. ThreadSanitizer is on: it understands memory orders, so a wrong one is reported as a data race.
#include <atomic>
#include <iostream>
#include <string>
#include <thread>
#include <vector>

#include "atomics.hpp"

int main() {
    Counter counter;
    SpinLock lock;
    int protected_by_the_lock = 0;
    OneShot<std::string> greeting;
    LockFreeStack<int> stack;

    {
        std::vector<std::jthread> threads;
        for (int t = 0; t < 4; ++t) {
            threads.emplace_back([&, t] {
                for (int i = 0; i < 500; ++i) {
                    counter.add(1);
                    {
                        std::lock_guard<SpinLock> guard(lock);
                        ++protected_by_the_lock;
                    }
                    stack.push(t * 1000 + i);
                }
            });
        }
        threads.emplace_back([&] { greeting.set("hello from another thread"); });
    }
    std::cout << "counter: " << counter.value() << " (expected 2000)\n";
    std::cout << "protected by the spin lock: " << protected_by_the_lock << " (expected 2000)\n";
    std::cout << "one-shot: " << greeting.get() << "\n";
    std::cout << "stack holds " << stack.drain().size() << " values (expected 2000)\n";

    std::atomic<int> a{5};
    std::cout << "fetch_double(5) returned " << fetch_double(a) << " and left " << a.load() << "\n";
}
