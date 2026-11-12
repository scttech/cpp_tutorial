#pragma once
// YOUR RATE LIMITERS. Time is a logical clock: a number of ticks (Tick), supplied by the caller on every call. All arithmetic is exact integer arithmetic.
//
// TokenBucket(capacity, refill_tokens, period, start_full = true)
//   A bucket holds up to `capacity` tokens and gains `refill_tokens` tokens EVERY `period` ticks, continuously (3 tokens per 10 ticks means 0.3 tokens per tick: after
//   7 ticks it has gained 2.1 tokens and the 0.1 must NOT be lost). Keep the balance in units of 1/period of a token: a token is `period` units and each elapsed tick adds
//   `refill_tokens` units; the balance never exceeds capacity * period. A bucket that starts full has capacity tokens, otherwise 0.
//   try_acquire(now, n = 1)   takes n tokens if the bucket has them: true. Otherwise takes nothing: false. n == 0 is always true; n > capacity is always false.
//   available(now)            whole tokens available (the balance divided by period, rounded down)
//   wait_time(now, n = 1)     the number of ticks until n tokens are available: 0 if they are now; the exact smallest number of ticks otherwise (round UP, never promise
//                             a token a tick too early); nullopt if it will never happen (n > capacity, or refill_tokens == 0 with too few tokens).
//   full(now)                 is the bucket at capacity?
//   Every call first brings the balance up to date: ticks since the last update * refill_tokens units, capped. Time is measured from the last UPDATE, whether or not the
//   request succeeded (updating only on success would count the same interval again and again). If `now` is smaller than the last time seen, no time passes (the clock
//   went backwards: ignore it, do not refill, do not go negative, and do not move the last time back). A huge gap must saturate at full, never wrap around.
//
// SlidingWindowLog(limit, window)    at most `limit` accepted events in any window of `window` ticks. An event at time t counts at time `now` when t > now - window
//   (equivalently t + window > now): after exactly `window` ticks it no longer counts.
//   try_acquire(now)    if fewer than `limit` events count: record the event at `now`, true. A DENIED event is not recorded. limit == 0 never allows.
//   count(now)          how many accepted events count at `now`
//   retry_after(now)    ticks until an event would be accepted: 0 if one is accepted now; otherwise until the oldest counted event leaves the window; nullopt if limit == 0
//   As with the bucket, a `now` earlier than the newest recorded event is treated as that event's time. Memory is at most `limit` timestamps.
//
// KeyedLimiter<Key>(capacity, refill_tokens, period)    one TokenBucket (starting full) per key, created on first use.
//   try_acquire(key, now, n = 1), size() (keys tracked)
//   evict_idle(now)     removes every key whose bucket is full at `now` (a full bucket is indistinguishable from a new one) and returns how many were removed.
#include <cstddef>
#include <cstdint>
#include <deque>
#include <optional>
#include <unordered_map>
#include <utility>

namespace ratelimit {

using Tick = std::uint64_t;

class TokenBucket {
public:
    TokenBucket(std::uint64_t capacity, std::uint64_t refill_tokens, Tick period, bool start_full = true)
        : capacity_(capacity), refill_(refill_tokens), period_(period == 0 ? 1 : period), balance_(start_full ? capacity * (period == 0 ? 1 : period) : 0) {}

    bool try_acquire(Tick now, std::uint64_t n = 1) {
        (void)now;
        (void)n;
        return false;  // TODO
    }
    std::uint64_t available(Tick now) {
        (void)now;
        return 0;  // TODO
    }
    std::optional<Tick> wait_time(Tick now, std::uint64_t n = 1) {
        (void)now;
        (void)n;
        return std::nullopt;  // TODO
    }
    bool full(Tick now) {
        (void)now;
        return false;  // TODO
    }

private:
    std::uint64_t capacity_, refill_;
    Tick period_;
    std::uint64_t balance_;  // in units of 1/period of a token
    Tick last_ = 0;
};

class SlidingWindowLog {
public:
    SlidingWindowLog(std::uint64_t limit, Tick window) : limit_(limit), window_(window) {}
    bool try_acquire(Tick now) {
        (void)now;
        return false;  // TODO
    }
    std::size_t count(Tick now) {
        (void)now;
        return 0;  // TODO
    }
    std::optional<Tick> retry_after(Tick now) {
        (void)now;
        return std::nullopt;  // TODO
    }

private:
    std::uint64_t limit_;
    Tick window_;
    std::deque<Tick> times_;
};

template <class Key>
class KeyedLimiter {
public:
    KeyedLimiter(std::uint64_t capacity, std::uint64_t refill_tokens, Tick period) : capacity_(capacity), refill_(refill_tokens), period_(period) {}
    bool try_acquire(const Key& key, Tick now, std::uint64_t n = 1) {
        (void)key;
        (void)now;
        (void)n;
        return false;  // TODO
    }
    std::size_t size() const { return 0; }  // TODO
    std::size_t evict_idle(Tick now) {
        (void)now;
        return 0;  // TODO
    }

private:
    std::uint64_t capacity_, refill_;
    Tick period_;
    // TODO: the per-key buckets
};

}  // namespace ratelimit
