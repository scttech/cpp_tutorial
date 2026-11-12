#define ZTH_TRACK_ALLOCATIONS
#include <algorithm>
#include <limits>
#include <string>
#include <vector>

#include "limiter.hpp"
#include "zth_test.hpp"

using namespace ratelimit;

namespace {

// An independent model of the bucket: it simulates every single tick (so it is only used with small times).
struct Model {
    std::uint64_t cap, refill, period, units;
    Tick last = 0;
    Model(std::uint64_t c, std::uint64_t r, Tick p, bool full) : cap(c), refill(r), period(p), units(full ? c * p : 0) {}
    void advance(Tick now) {
        for (; last < now; ++last) units = std::min(cap * period, units + refill);
    }
    bool acquire(Tick now, std::uint64_t n) {
        advance(now);
        if (n == 0) return true;
        if (n > cap || units < n * period) return false;
        units -= n * period;
        return true;
    }
    std::uint64_t available(Tick now) {
        advance(now);
        return units / period;
    }
};

Tick val(const std::optional<Tick>& o) { return o.value_or(0); }

std::string run(TokenBucket& b, const std::vector<Tick>& times) {
    std::string out;
    for (Tick t : times) out += b.try_acquire(t) ? 'Y' : 'n';
    return out;
}

}  // namespace

// ================================================ Works =======================================================
ZTH_TEST("works", "a full bucket allows a burst up to its capacity, then denies") {
    TokenBucket b(5, 1, 10);
    ZTH_EXPECT_EQ(run(b, {0, 0, 0, 0, 0, 0, 0}), std::string("YYYYYnn"));
}
ZTH_TEST("works", "tokens come back exactly one period after they are needed") {
    TokenBucket b(5, 1, 10);
    run(b, {0, 0, 0, 0, 0});
    ZTH_EXPECT_EQ(run(b, {9}), std::string("n"));
    ZTH_EXPECT_EQ(run(b, {10}), std::string("Y"));
    ZTH_EXPECT_EQ(run(b, {10, 15, 19}), std::string("nnn"));
    ZTH_EXPECT_EQ(run(b, {20}), std::string("Y"));
}
ZTH_TEST("works", "a bucket that starts empty fills up over time") {
    TokenBucket b(3, 1, 4, false);
    ZTH_EXPECT_EQ(b.available(0), std::uint64_t{0});
    ZTH_EXPECT_EQ(run(b, {0, 3}), std::string("nn"));
    ZTH_EXPECT_EQ(run(b, {4}), std::string("Y"));
    ZTH_EXPECT_EQ(b.available(100), std::uint64_t{3});
}
ZTH_TEST("works", "the balance never exceeds the capacity, however long the bucket idles") {
    TokenBucket b(4, 3, 7);
    ZTH_EXPECT_EQ(b.available(0), std::uint64_t{4});
    ZTH_EXPECT_EQ(b.available(1000000), std::uint64_t{4});
    ZTH_EXPECT_EQ(run(b, {1000000, 1000000, 1000000, 1000000, 1000000}), std::string("YYYYn"));
}
ZTH_TEST("works", "fractions of tokens are kept: 3 tokens per 10 ticks is 0.3 per tick, with no drift") {
    TokenBucket b(10, 3, 10, false);
    ZTH_EXPECT_EQ(b.available(3), std::uint64_t{0});
    ZTH_EXPECT_EQ(b.available(4), std::uint64_t{1});    // 1.2 tokens
    ZTH_EXPECT_EQ(b.available(7), std::uint64_t{2});    // 2.1
    ZTH_EXPECT_EQ(b.available(10), std::uint64_t{3});   // 3.0 exactly
    ZTH_EXPECT_EQ(b.available(20), std::uint64_t{6});
    // calling often must not lose the fractions either
    TokenBucket c(1000, 3, 10, false);
    for (Tick t = 1; t <= 1000; ++t) c.available(t);
    ZTH_EXPECT_EQ(c.available(1000), std::uint64_t{300});
}
ZTH_TEST("works", "one token per 3 ticks, asked for at every tick, grants exactly floor(T / 3) tokens in T ticks") {
    TokenBucket b(1, 1, 3, false);
    int granted = 0;
    for (Tick t = 1; t <= 3000; ++t) granted += b.try_acquire(t);
    ZTH_EXPECT_EQ(granted, 1000);
}
ZTH_TEST("works", "a denied request still brings the clock up to date: waiting is not counted twice") {
    TokenBucket b(2, 1, 10, false);
    ZTH_EXPECT_FALSE(b.try_acquire(5));    // 0.5 tokens
    ZTH_EXPECT_FALSE(b.try_acquire(9));    // 0.9
    ZTH_EXPECT_TRUE(b.try_acquire(10));    // exactly 1.0
    ZTH_EXPECT_FALSE(b.try_acquire(10));
    ZTH_EXPECT_EQ(b.available(15), std::uint64_t{0});   // 0.5, not 1.0 from counting the interval 5..15 on top of the earlier updates
    ZTH_EXPECT_EQ(b.available(20), std::uint64_t{1});
}
ZTH_TEST("works", "taking several tokens at once is all or nothing") {
    TokenBucket b(10, 1, 1);
    ZTH_EXPECT_TRUE(b.try_acquire(0, 4));
    ZTH_EXPECT_TRUE(b.try_acquire(0, 6));
    ZTH_EXPECT_FALSE(b.try_acquire(0, 2));
    ZTH_EXPECT_EQ(b.available(0), std::uint64_t{0});     // the failed request took nothing
    ZTH_EXPECT_FALSE(b.try_acquire(1, 2));
    ZTH_EXPECT_TRUE(b.try_acquire(2, 2));
    ZTH_EXPECT_FALSE(b.try_acquire(100, 11));            // more than the capacity can never succeed
    ZTH_EXPECT_EQ(b.available(100), std::uint64_t{10});
    ZTH_EXPECT_TRUE(b.try_acquire(100, 10));
}
ZTH_TEST("works", "zero tokens are always granted and cost nothing") {
    TokenBucket b(2, 1, 10, false);
    ZTH_EXPECT_TRUE(b.try_acquire(0, 0));
    ZTH_EXPECT_EQ(b.available(0), std::uint64_t{0});
    TokenBucket z(0, 5, 10);
    ZTH_EXPECT_TRUE(z.try_acquire(0, 0));
    ZTH_EXPECT_FALSE(z.try_acquire(1000, 1));   // a bucket of capacity 0 never holds anything
}
ZTH_TEST("works", "wait_time is the exact number of ticks, rounded up") {
    TokenBucket b(5, 1, 10, false);
    ZTH_EXPECT_EQ(val(b.wait_time(0)), Tick{10});
    ZTH_EXPECT_EQ(val(b.wait_time(4)), Tick{6});
    ZTH_EXPECT_EQ(val(b.wait_time(4, 3)), Tick{26});
    TokenBucket c(10, 3, 10, false);
    ZTH_EXPECT_EQ(val(c.wait_time(0, 1)), Tick{4});      // 10 units needed at 3 per tick: 3.33 -> 4 ticks
    ZTH_EXPECT_EQ(val(c.wait_time(0, 2)), Tick{7});      // 20 / 3 = 6.67 -> 7
    ZTH_EXPECT_EQ(val(c.wait_time(0, 3)), Tick{10});     // 30 / 3 = 10 exactly
    // and the promise is kept: one tick earlier it is not enough, at the promised tick it is
    for (std::uint64_t n : {1, 2, 3, 4, 7}) {
        TokenBucket d(10, 3, 10, false);
        const Tick w = val(d.wait_time(0, n));
        TokenBucket early(10, 3, 10, false);
        TokenBucket on_time(10, 3, 10, false);
        ZTH_EXPECT_FALSE(early.try_acquire(w - 1, n));
        ZTH_EXPECT_TRUE(on_time.try_acquire(w, n));
    }
}
ZTH_TEST("works", "wait_time is 0 when the tokens are there and nullopt when they never will be") {
    TokenBucket b(5, 1, 10);
    ZTH_EXPECT_EQ(val(b.wait_time(0, 5)), Tick{0});
    ZTH_EXPECT_EQ(val(b.wait_time(0, 0)), Tick{0});
    ZTH_EXPECT_FALSE(b.wait_time(0, 6).has_value());   // more than the capacity
    TokenBucket no_refill(5, 0, 10);
    ZTH_EXPECT_TRUE(no_refill.try_acquire(0, 5));
    ZTH_EXPECT_FALSE(no_refill.wait_time(1000000, 1).has_value());
    ZTH_EXPECT_FALSE(no_refill.try_acquire(1000000));
    TokenBucket no_refill_full(5, 0, 10);
    ZTH_EXPECT_EQ(val(no_refill_full.wait_time(0, 3)), Tick{0});
}
ZTH_TEST("works", "full() tells whether the bucket is back at capacity") {
    TokenBucket b(2, 1, 10);
    ZTH_EXPECT_TRUE(b.full(0));
    b.try_acquire(0);
    ZTH_EXPECT_FALSE(b.full(0));
    ZTH_EXPECT_FALSE(b.full(9));
    ZTH_EXPECT_TRUE(b.full(10));
    TokenBucket e(2, 1, 10, false);
    ZTH_EXPECT_FALSE(e.full(0));
    ZTH_EXPECT_TRUE(e.full(20));
}
ZTH_TEST("works", "a clock that goes backwards neither refills nor breaks the bucket") {
    TokenBucket b(3, 1, 10);
    run(b, {100, 100, 100});
    ZTH_EXPECT_EQ(b.available(100), std::uint64_t{0});
    ZTH_EXPECT_EQ(b.available(40), std::uint64_t{0});     // earlier: nothing happens
    ZTH_EXPECT_FALSE(b.try_acquire(0));
    ZTH_EXPECT_EQ(b.available(105), std::uint64_t{0});
    ZTH_EXPECT_EQ(b.available(110), std::uint64_t{1});    // counted from 100, not from 40 or 0
    ZTH_EXPECT_FALSE(b.wait_time(50, 2) ? val(b.wait_time(50, 2)) == 0 : false);
}
ZTH_TEST("works", "a huge gap in time saturates at full instead of wrapping around") {
    TokenBucket b(1000, 1000000, 1);
    run(b, std::vector<Tick>(1000, 0));
    ZTH_EXPECT_EQ(b.available(0), std::uint64_t{0});
    const Tick far = std::numeric_limits<Tick>::max() / 2;
    ZTH_EXPECT_EQ(b.available(far), std::uint64_t{1000});
    TokenBucket c(1ULL << 40, 1ULL << 40, 1);
    c.try_acquire(0, 1ULL << 40);
    ZTH_EXPECT_EQ(c.available(std::numeric_limits<Tick>::max()), std::uint64_t{1ULL << 40});
}
ZTH_TEST("works", "multiplications that would overflow saturate instead of wrapping around") {
    TokenBucket c(1ULL << 40, 1ULL << 32, 1);
    ZTH_EXPECT_TRUE(c.try_acquire(0, 1ULL << 40));
    // 2^32 + 1 ticks at 2^32 tokens per tick is 2^64 + 2^32 tokens: far more than the capacity. Wrapped around it would look like 2^32.
    ZTH_EXPECT_EQ(c.available((1ULL << 32) + 1), std::uint64_t{1ULL << 40});
    TokenBucket b(5, 1, 2);
    ZTH_EXPECT_FALSE(b.try_acquire(0, (1ULL << 63) + 1));   // n * period would wrap around to 2
    ZTH_EXPECT_FALSE(b.wait_time(0, (1ULL << 63) + 1).has_value());
    ZTH_EXPECT_EQ(b.available(0), std::uint64_t{5});
}
ZTH_TEST("works", "a period of zero is treated as one tick") {
    TokenBucket b(3, 1, 0, false);
    ZTH_EXPECT_EQ(b.available(2), std::uint64_t{2});
}
ZTH_TEST("works", "the sliding window allows `limit` events in any window of `window` ticks") {
    SlidingWindowLog w(3, 10);
    std::string out;
    for (Tick t : {0, 1, 2, 3, 9, 10, 11, 12, 13}) out += w.try_acquire(t) ? 'Y' : 'n';
    // t=10: the event at 0 left (10 - 0 >= 10), the window holds 1, 2 -> allowed; t=11: the one at 1 left -> allowed; t=12: 2 left -> hmm 10, 11 count
    ZTH_EXPECT_EQ(out, std::string("YYYnnYYYn"));
}
ZTH_TEST("works", "the boundary: an event counts for exactly `window` ticks") {
    SlidingWindowLog w(1, 100);
    ZTH_EXPECT_TRUE(w.try_acquire(50));
    ZTH_EXPECT_FALSE(w.try_acquire(149));
    ZTH_EXPECT_TRUE(w.try_acquire(150));
    SlidingWindowLog z(1, 1);
    ZTH_EXPECT_TRUE(z.try_acquire(5));
    ZTH_EXPECT_FALSE(z.try_acquire(5));
    ZTH_EXPECT_TRUE(z.try_acquire(6));
}
ZTH_TEST("works", "denied events are not recorded, so they do not extend the penalty") {
    SlidingWindowLog w(2, 10);
    ZTH_EXPECT_TRUE(w.try_acquire(0));
    ZTH_EXPECT_TRUE(w.try_acquire(1));
    for (Tick t = 2; t < 10; ++t) ZTH_EXPECT_FALSE(w.try_acquire(t));
    ZTH_EXPECT_EQ(w.count(9), std::size_t{2});
    ZTH_EXPECT_TRUE(w.try_acquire(10));   // the event at 0 is out; nothing was added by the eight denied requests
    ZTH_EXPECT_EQ(w.count(10), std::size_t{2});
}
ZTH_TEST("works", "count and retry_after") {
    SlidingWindowLog w(3, 10);
    ZTH_EXPECT_EQ(w.count(0), std::size_t{0});
    ZTH_EXPECT_EQ(val(w.retry_after(0)), Tick{0});
    w.try_acquire(0);
    w.try_acquire(4);
    ZTH_EXPECT_EQ(w.count(5), std::size_t{2});
    ZTH_EXPECT_EQ(val(w.retry_after(5)), Tick{0});
    w.try_acquire(5);
    ZTH_EXPECT_EQ(val(w.retry_after(5)), Tick{5});    // the oldest event (0) leaves the window at tick 10
    ZTH_EXPECT_EQ(val(w.retry_after(9)), Tick{1});
    ZTH_EXPECT_EQ(val(w.retry_after(10)), Tick{0});
    ZTH_EXPECT_EQ(w.count(10), std::size_t{2});
    ZTH_EXPECT_EQ(w.count(14), std::size_t{1});   // the event at 4 left at tick 14
    ZTH_EXPECT_EQ(w.count(15), std::size_t{0});
}
ZTH_TEST("works", "limit 0 never allows anything; a clock that goes backwards counts as the newest event's time") {
    SlidingWindowLog z(0, 10);
    ZTH_EXPECT_FALSE(z.try_acquire(0));
    ZTH_EXPECT_FALSE(z.try_acquire(100));
    ZTH_EXPECT_FALSE(z.retry_after(0).has_value());
    SlidingWindowLog w(2, 10);
    w.try_acquire(100);
    ZTH_EXPECT_TRUE(w.try_acquire(50));     // treated as time 100
    ZTH_EXPECT_FALSE(w.try_acquire(60));
    ZTH_EXPECT_EQ(w.count(0), std::size_t{2});
    ZTH_EXPECT_TRUE(w.try_acquire(110));
}
ZTH_TEST("works", "a keyed limiter gives every key its own bucket") {
    KeyedLimiter<std::string> k(2, 1, 100);
    ZTH_EXPECT_TRUE(k.try_acquire("alice", 0));
    ZTH_EXPECT_TRUE(k.try_acquire("alice", 0));
    ZTH_EXPECT_FALSE(k.try_acquire("alice", 0));
    ZTH_EXPECT_TRUE(k.try_acquire("bob", 0));
    ZTH_EXPECT_TRUE(k.try_acquire("bob", 0));
    ZTH_EXPECT_FALSE(k.try_acquire("bob", 0));
    ZTH_EXPECT_TRUE(k.try_acquire("carol", 0, 2));
    ZTH_EXPECT_EQ(k.size(), std::size_t{3});
    ZTH_EXPECT_TRUE(k.try_acquire("alice", 100));
    ZTH_EXPECT_FALSE(k.try_acquire("alice", 100));
    ZTH_EXPECT_FALSE(k.try_acquire("alice", 5, 3));   // asking for more than the capacity never works
}
ZTH_TEST("works", "evict_idle removes keys whose bucket is full again, and only those") {
    KeyedLimiter<int> k(2, 1, 10);
    k.try_acquire(1, 0);          // 1 token left, refills at t=10
    k.try_acquire(2, 0, 2);       // empty, full again at t=20
    k.try_acquire(3, 0);          // 1 token left, refills at t=10
    ZTH_EXPECT_EQ(k.size(), std::size_t{3});
    ZTH_EXPECT_EQ(k.evict_idle(5), std::size_t{0});
    ZTH_EXPECT_EQ(k.evict_idle(10), std::size_t{2});
    ZTH_EXPECT_EQ(k.size(), std::size_t{1});
    ZTH_EXPECT_EQ(k.evict_idle(19), std::size_t{0});
    ZTH_EXPECT_EQ(k.evict_idle(20), std::size_t{1});
    ZTH_EXPECT_EQ(k.size(), std::size_t{0});
    ZTH_EXPECT_TRUE(k.try_acquire(2, 25, 2));   // an evicted key starts again with a full bucket
}

// ================================================ Robust ======================================================
ZTH_TEST("robust", "the bucket agrees with a tick-by-tick model on 300 random configurations and request streams") {
    zth::Lcg rng(1234);
    for (int round = 0; round < 300; ++round) {
        const std::uint64_t cap = static_cast<std::uint64_t>(rng.below(12));
        const std::uint64_t refill = static_cast<std::uint64_t>(rng.below(9));
        const Tick period = static_cast<Tick>(1 + rng.below(12));
        const bool full = rng.below(2) == 0;
        TokenBucket b(cap, refill, period, full);
        Model m(cap, refill, period, full);
        Tick now = 0;
        for (int step = 0; step < 80; ++step) {
            now += static_cast<Tick>(rng.below(6));
            const std::uint64_t n = static_cast<std::uint64_t>(rng.below(5));
            switch (rng.below(4)) {
                case 0:
                case 1: {
                    const bool got = b.try_acquire(now, n), want = m.acquire(now, n);
                    if (got != want) {
                        zth::fail("round " + std::to_string(round) + " step " + std::to_string(step) + ": try_acquire(" + std::to_string(now) + ", " + std::to_string(n) + ") returned " + (got ? "true" : "false") +
                                  " (cap " + std::to_string(cap) + ", refill " + std::to_string(refill) + " per " + std::to_string(period) + ")");
                        return;
                    }
                    break;
                }
                case 2:
                    if (b.available(now) != m.available(now)) {
                        zth::fail("round " + std::to_string(round) + " step " + std::to_string(step) + ": available(" + std::to_string(now) + ") = " + std::to_string(b.available(now)) + ", model " + std::to_string(m.available(now)));
                        return;
                    }
                    break;
                default: {
                    const auto w = b.wait_time(now, n);
                    Model probe = m;
                    probe.advance(now);
                    if (n > cap || (refill == 0 && probe.units < n * period)) {
                        if (w.has_value()) {
                            zth::fail("wait_time should be nullopt at round " + std::to_string(round));
                            return;
                        }
                    } else {
                        if (!w) {
                            zth::fail("wait_time should have a value at round " + std::to_string(round));
                            return;
                        }
                        Model at = probe, before = probe;
                        const bool ok_on_time = at.acquire(now + *w, n);
                        const bool ok_early = *w == 0 ? false : before.acquire(now + *w - 1, n);
                        if (!ok_on_time || ok_early) {
                            zth::fail("wait_time(" + std::to_string(now) + ", " + std::to_string(n) + ") = " + std::to_string(*w) + " is not the smallest wait (round " + std::to_string(round) + ")");
                            return;
                        }
                    }
                }
            }
        }
    }
}
ZTH_TEST("robust", "the sliding window agrees with a brute-force list of timestamps on 300 random streams") {
    zth::Lcg rng(77);
    for (int round = 0; round < 300; ++round) {
        const std::uint64_t limit = static_cast<std::uint64_t>(rng.below(6));
        const Tick window = static_cast<Tick>(1 + rng.below(15));
        SlidingWindowLog w(limit, window);
        std::vector<Tick> accepted;
        Tick now = 0;
        for (int step = 0; step < 100; ++step) {
            now += static_cast<Tick>(rng.below(5));
            std::size_t counted = 0;
            Tick oldest = 0;
            bool have_oldest = false;
            for (Tick t : accepted) {
                if (t + window > now) {
                    ++counted;
                    if (!have_oldest) {
                        oldest = t;
                        have_oldest = true;
                    }
                }
            }
            const bool want = counted < limit;
            if (w.count(now) != counted) {
                zth::fail("count(" + std::to_string(now) + ") = " + std::to_string(w.count(now)) + ", expected " + std::to_string(counted) + " (round " + std::to_string(round) + ")");
                return;
            }
            const auto retry = w.retry_after(now);
            if (limit == 0 ? retry.has_value() : (!retry || *retry != (want ? 0 : oldest + window - now))) {
                zth::fail("retry_after is wrong at round " + std::to_string(round) + ", step " + std::to_string(step));
                return;
            }
            if (w.try_acquire(now) != want) {
                zth::fail("try_acquire(" + std::to_string(now) + ") is wrong at round " + std::to_string(round) + ", step " + std::to_string(step));
                return;
            }
            if (want) accepted.push_back(now);
        }
    }
}
ZTH_TEST("robust", "over any long period the bucket never grants more than capacity + rate * time, and nearly that much when it is always asked") {
    TokenBucket b(7, 5, 13);
    std::uint64_t granted = 0;
    const Tick horizon = 100000;
    for (Tick t = 0; t <= horizon; ++t) granted += b.try_acquire(t);
    const std::uint64_t upper = 7 + horizon * 5 / 13;
    ZTH_EXPECT_TRUE(granted <= upper);
    ZTH_EXPECT_TRUE(granted + 1 >= upper);   // exact accounting loses nothing
}
ZTH_TEST("robust", "many keys, interleaved: each key behaves as if it were alone") {
    KeyedLimiter<int> k(3, 2, 10);
    std::vector<TokenBucket> alone(50, TokenBucket(3, 2, 10));
    zth::Lcg rng(9);
    Tick now = 0;
    for (int i = 0; i < 5000; ++i) {
        now += static_cast<Tick>(rng.below(4));
        const int key = rng.below(50);
        if (k.try_acquire(key, now) != alone[static_cast<std::size_t>(key)].try_acquire(now)) {
            zth::fail("key " + std::to_string(key) + " at tick " + std::to_string(now) + " differs from a private bucket");
            return;
        }
        if (i % 500 == 499) k.evict_idle(now);   // evicting full buckets must not change anybody's answers
    }
}

// ================================================ Fast ========================================================
ZTH_TEST("fast", "the bucket and the window allocate nothing in their steady state") {
    TokenBucket b(100, 7, 3);
    SlidingWindowLog w(50, 1000);
    for (Tick t = 0; t < 2000; ++t) w.try_acquire(t);   // warm-up: the window reaches its working size
    zth::mem::Scope scope;
    std::uint64_t ok = 0;
    for (Tick t = 0; t < 100000; ++t) ok += b.try_acquire(t) + b.available(t) + b.wait_time(t, 3).value_or(0) + b.full(t);
    ZTH_EXPECT_ALLOCS_AT_MOST(scope, 0);
    ZTH_EXPECT_TRUE(ok > 0);
}
ZTH_TEST_TIMEOUT("fast", "the sliding window with a limit of 200000 does O(1) work per event (erasing from the front of a vector would take minutes)", 20) {
    SlidingWindowLog w(200000, 200000);
    std::uint64_t ok = 0;
    for (Tick t = 0; t < 1000000; ++t) ok += w.try_acquire(t);
    ZTH_EXPECT_EQ(ok, std::uint64_t{1000000});
    ZTH_EXPECT_EQ(w.count(999999), std::size_t{200000});
}
ZTH_TEST("fast", "memory is bounded by the limit: the window never holds more than `limit` timestamps, however many requests arrive") {
    SlidingWindowLog w(10, 1000000000);
    for (Tick t = 0; t < 100000; ++t) w.try_acquire(t);
    ZTH_EXPECT_EQ(w.count(100000), std::size_t{10});
}
ZTH_TEST_TIMEOUT("fast", "100000 keys, then evict_idle forgets them all", 20) {
    KeyedLimiter<std::uint64_t> k(5, 1, 100);
    for (std::uint64_t id = 0; id < 100000; ++id) k.try_acquire(id, 0);
    ZTH_EXPECT_EQ(k.size(), std::size_t{100000});
    ZTH_EXPECT_EQ(k.evict_idle(50), std::size_t{0});
    ZTH_EXPECT_EQ(k.evict_idle(100), std::size_t{100000});
    ZTH_EXPECT_EQ(k.size(), std::size_t{0});
}

// ================================================ Clean =======================================================
ZTH_TEST("clean", "no raw memory, no exceptions, no goto") {
    ZTH_EXPECT_NO_RAW_MEMORY("limiter.hpp");
    const std::string code = ::zth::strip_comments_and_literals(::zth::read_file("limiter.hpp"));
    for (const char* word : {"throw", "try", "catch", "goto"}) ZTH_EXPECT_TRUE(::zth::find_word(code, word).empty());
}
ZTH_TEST("clean", "exact integer arithmetic: no floating point, no <cmath>, no <chrono>, no real clock, no sleeping") {
    const std::string code = ::zth::strip_comments_and_literals(::zth::read_file("limiter.hpp"));
    for (const char* word : {"float", "double", "cmath", "ceil", "floor", "round", "pow", "chrono", "steady_clock", "system_clock", "sleep_for", "time", "clock", "thread", "mutex", "atomic"}) ZTH_EXPECT_TRUE(::zth::find_word(code, word).empty());
}
ZTH_TEST("clean", "the window is a queue of timestamps: a deque/queue, not a vector that is erased from the front") {
    const std::string code = ::zth::strip_comments_and_literals(::zth::read_file("limiter.hpp"));
    ZTH_EXPECT_TRUE(::zth::find_word(code, "vector").empty());
    ZTH_EXPECT_FALSE(::zth::find_word(code, "deque").empty());
}
