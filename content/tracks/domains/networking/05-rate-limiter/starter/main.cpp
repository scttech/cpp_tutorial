// Rate limiting on a logical clock (ticks = milliseconds, say): a token bucket, a sliding window, and a per-client limiter. Read-only. Press Run.
#include <cstdio>
#include <string>

#include "limiter.hpp"

int main() {
    // a bucket of 5 tokens that refills 1 token per 100 ticks
    ratelimit::TokenBucket bucket(5, 1, 100);
    std::printf("burst of 7 requests at t=0: ");
    for (int i = 0; i < 7; ++i) std::printf("%s ", bucket.try_acquire(0) ? "ok" : "DENIED");
    const auto wait = bucket.wait_time(0);
    std::printf("\nnext token in %llu ticks; at t=99: %s, at t=100: %s\n", static_cast<unsigned long long>(wait.value_or(0)), bucket.try_acquire(99) ? "ok" : "DENIED", bucket.try_acquire(100) ? "ok" : "DENIED");

    ratelimit::SlidingWindowLog window(3, 1000);   // at most 3 requests in any 1000 ticks
    for (ratelimit::Tick t : {0, 100, 200, 300, 999, 1000, 1100}) std::printf("window t=%-5llu %s\n", static_cast<unsigned long long>(t), window.try_acquire(t) ? "ok" : "DENIED");

    ratelimit::KeyedLimiter<std::string> per_client(2, 1, 1000);
    const char* clients[] = {"alice", "bob", "alice", "alice", "bob", "carol"};
    for (const char* c : clients) std::printf("%-6s %s\n", c, per_client.try_acquire(c, 0) ? "ok" : "DENIED");
    std::printf("tracking %zu clients; after idling 5000 ticks, %zu are forgotten\n", per_client.size(), per_client.evict_idle(5000));
}
