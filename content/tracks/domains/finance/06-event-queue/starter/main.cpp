// A feed-handler thread produces events, a strategy thread consumes them, with no lock between them. Read-only. Press Run.
#include <cstdio>
#include <thread>

#include "spsc_queue.hpp"
#include "types.hpp"

int main() {
    evq::SpscQueue<evq::Event, 1024> queue;
    constexpr std::uint64_t kEvents = 200000;

    std::thread producer([&] {
        for (std::uint64_t i = 1; i <= kEvents; ++i) {
            while (!queue.try_push(evq::Event::make(i))) std::this_thread::yield();  // full: wait for the consumer
        }
    });

    std::uint64_t received = 0, expected = 1, out_of_order = 0, torn = 0;
    evq::Event e;
    while (received < kEvents) {
        if (!queue.try_pop(e)) {
            std::this_thread::yield();  // empty
            continue;
        }
        ++received;
        if (e.seq != expected++) ++out_of_order;
        if (!e.consistent()) ++torn;
    }
    producer.join();
    std::printf("received %llu events, out of order: %llu, torn: %llu\n", static_cast<unsigned long long>(received), static_cast<unsigned long long>(out_of_order), static_cast<unsigned long long>(torn));
    std::printf("queue capacity %zu, left over: %zu\n", queue.capacity(), queue.size_approx());
}
