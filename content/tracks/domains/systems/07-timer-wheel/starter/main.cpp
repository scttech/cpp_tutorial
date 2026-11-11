// Connection timeouts on a hashed timer wheel, driven by a logical clock. Read-only. Press Run.
#include <cstdio>
#include <string>

#include "timer_wheel.hpp"

int main() {
    wheel::TimerWheel timers(64);   // 64 slots: one tick = whatever the caller says (say, 10 ms)
    auto say = [&](const char* what) { std::printf("[tick %3llu] %s\n", static_cast<unsigned long long>(timers.now()), what); };

    timers.schedule(30, [&] { say("connection 1 idle for 30 ticks: closing"); });
    const auto c2 = timers.schedule(50, [&] { say("connection 2 timed out"); });
    timers.schedule(100, [&] {
        say("heartbeat");
        timers.schedule(100, [&] { say("second heartbeat (scheduled from the first)"); });   // callbacks may schedule more timers
    });
    timers.schedule(200, [&] { say("a timer far beyond one trip around the wheel"); });
    timers.schedule(10, [&] { say("connection 2 answered in time: cancelling its timeout"); timers.cancel(c2); });

    std::printf("pending: %zu\n", timers.pending());
    const std::size_t fired = timers.advance(150);
    std::printf("advanced to tick %llu, %zu timers fired, %zu pending, next deadline: %llu\n", static_cast<unsigned long long>(timers.now()), fired, timers.pending(),
                static_cast<unsigned long long>(timers.next_deadline().value_or(0)));
    timers.advance(100);
    std::printf("tick %llu, pending %zu\n", static_cast<unsigned long long>(timers.now()), timers.pending());
}
