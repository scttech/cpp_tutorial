# Rate Limiters

> **You're an engineer on the networking team.** Your API gets 50 requests per second from a well-behaved client and 5000 from a broken script. Without a limit, one client takes down everyone. You need two classic limiters: the **token bucket** (allow bursts, but a long-run average rate) and the **sliding window log** (never more than N in any window), plus a way to keep one of them **per client** without leaking memory for the million clients that came once. And you must be able to test them, which means the clock is a parameter: `try_acquire(now)`, not `steady_clock::now()` inside.

## Token bucket

A bucket of `capacity` tokens that gains `refill` tokens every `period` ticks; each request takes tokens. A full bucket lets a burst of `capacity` through, then requests are served at the refill rate.

The details that decide correctness:

- **Exact arithmetic.** "3 tokens per 10 ticks" is 0.3 tokens per tick. With floating point, `0.1 + 0.2 != 0.3` and a long-running limiter drifts. Instead store the balance in units of 1/`period` of a token: a token is `period` units, each elapsed tick adds `refill` units. Everything stays an integer, and over 100000 ticks the bucket grants *exactly* what the rate allows.
- **Refill lazily, on every call, from the last *update*.** There is no timer thread: when a call arrives, add `elapsed * refill` and set `last = now`. If you only advance `last` on success, a denied request leaves the interval uncounted and the next call counts it **again**: the bucket slowly mints free tokens.
- **Cap the balance** at `capacity * period`, and **saturate** the multiplication: `elapsed * refill` with a huge `elapsed` overflows `uint64_t` and wraps to a small number, so a bucket idle for a very long time would be *empty*. Compare `elapsed > room / refill` before multiplying.
- **Time can go backwards** (NTP step, a bad caller). `now - last` on unsigned types would wrap to a gigantic gap. If `now <= last`, nothing happens, and `last` must not move back.
- **`wait_time`** (what you put in a `Retry-After` header): the smallest number of ticks after which the request would succeed, which is `ceil(deficit / refill)`. Rounding down promises a token one tick early and the client retries into another denial.

## Sliding window log

Remember the timestamps of the last `limit` accepted events. An event is accepted if fewer than `limit` are still inside `(now - window, now]`. It is exact (no burst of `2 * limit` across a window boundary, unlike a fixed-window counter) and costs O(limit) memory. Details: an event counts for exactly `window` ticks; **denied events are not recorded** (otherwise a client that keeps hammering never recovers); use a **`std::deque`** so evicting from the front is O(1) (`vector::erase(begin())` is O(n) and makes the limiter quadratic).

## Per client

An `unordered_map<Key, TokenBucket>` with a bucket per client. A bucket that is **full again** is identical to a brand-new one, so `evict_idle(now)` can delete it and the map's size is bounded by the number of *recently active* clients instead of everyone ever seen.

## The tiers

- **Works**: bursts, exact refill timing, fractions without drift, 1-per-3 ticks gives exactly `floor(T/3)`, denied requests update the clock, multi-token requests, zero-cost requests, `wait_time` rounding and `nullopt`, a clock going backwards, a gap of 2^63 ticks, the window boundary, denied events, `retry_after`, per-key isolation, eviction.
- **Robust**: 300 random bucket configurations compared **tick by tick** with a simulation model (including that `wait_time` is the *smallest* correct wait), 300 random window streams compared with a brute-force list, a 100000-tick grant bound, 50 interleaved keys.
- **Fast**: no allocation in the steady state, a window of 200000 events in linear time, bounded memory, 100000 keys evicted.
- **Clean**: no raw memory or exceptions, no floating point or real clock, `deque` and no `vector` for the window.
