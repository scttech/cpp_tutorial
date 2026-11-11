// A demo of the Scheduler (read-only). Press Run. ThreadSanitizer is on.
#include <iostream>
#include <latch>
#include <string>
#include <vector>

#include "scheduler.hpp"

int main() {
    Scheduler scheduler(1);  // ONE worker: jobs run one at a time, so the order is easy to see
    std::latch release(1);
    auto blocker = scheduler.submit(Priority::Normal, [&] { release.wait(); });  // keeps the worker busy while we queue the others

    std::vector<std::string> order;
    std::mutex m;
    auto record = [&](std::string name) {
        return [&order, &m, name] {
            std::lock_guard lock(m);
            order.push_back(name);
        };
    };
    auto low = scheduler.submit(Priority::Low, record("low"));
    auto high = scheduler.submit(Priority::High, record("high"));
    auto normal = scheduler.submit(Priority::Normal, record("normal"));
    auto high2 = scheduler.submit(Priority::High, record("high-2"));
    auto cancelled = scheduler.submit(Priority::Normal, record("cancelled"));
    std::cout << "queued: " << scheduler.pending() << " jobs, cancel one: " << std::boolalpha << cancelled.cancel() << "\n";

    release.count_down();
    scheduler.wait_idle();
    std::cout << "ran in this order:";
    for (const auto& name : order) std::cout << ' ' << name;
    std::cout << "\n";
    try {
        cancelled.get();
    } catch (const JobCancelled& e) {
        std::cout << "the cancelled job says: " << e.what() << "\n";
    }

    auto answer = scheduler.submit(Priority::Normal, [] { return 6 * 7; });
    std::cout << "a job with a result: " << answer.get() << "\n";
    const auto stats = scheduler.stats();
    std::cout << "completed " << stats.completed << ", failed " << stats.failed << ", cancelled " << stats.cancelled << "\n";
}
