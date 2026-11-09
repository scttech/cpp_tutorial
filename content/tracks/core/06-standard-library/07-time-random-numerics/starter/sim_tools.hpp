#pragma once

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <format>
#include <functional>
#include <numeric>
#include <random>
#include <string>
#include <utility>
#include <vector>

// Time, randomness and numbers: the three places where a program that "looks right" is quietly wrong.
// Every function is a stub that compiles but is wrong: replace each body.

// ---- 1. std::chrono: durations carry their UNIT in the type ------------------------------------------------------------------------------
// TODO: the sum of the three durations, in milliseconds. Mixed units convert automatically (and exactly) to the finer one: no *1000 anywhere.
inline std::chrono::milliseconds total_time(std::chrono::minutes m, std::chrono::seconds s, std::chrono::milliseconds ms) {
    (void)m;
    (void)s;
    (void)ms;
    return std::chrono::milliseconds{0};
}

// TODO: the duration as a number of seconds with a fraction: to_fractional_seconds(1500ms) is 1.5. (std::chrono::duration<double> is "seconds as
// a double": converting to it never truncates. duration_cast to an integer duration TRUNCATES toward zero.)
inline double to_fractional_seconds(std::chrono::milliseconds d) {
    (void)d;
    return 0.0;
}

// TODO: "H h MM m SS s": hours (not limited to 24), minutes and seconds with two digits; the milliseconds are dropped (truncated).
//   pretty_duration(3723004ms) is "1h 02m 03s";  pretty_duration(0ms) is "0h 00m 00s";  a negative duration gets a '-' in front: "-0h 00m 01s".
// std::chrono::duration_cast<std::chrono::hours>(d), then subtract and cast again for the minutes and seconds; std::format("{:02}", n).
inline std::string pretty_duration(std::chrono::milliseconds d) {
    (void)d;
    return "";
}

// A stopwatch that reads time from ANY clock type (std::chrono::steady_clock in a program, a fake clock in a test): `Clock::now()` is the only thing
// it needs. Injecting the clock is how time-dependent code becomes testable and deterministic.
// TODO: start() remembers Clock::now(); elapsed() is the milliseconds since start() (duration_cast: truncated); lap() is the milliseconds since the
// previous lap() (or since start()), and the next lap begins now.
template <class Clock>
class Stopwatch {
public:
    void start() {}
    std::chrono::milliseconds elapsed() const { return std::chrono::milliseconds{0}; }
    std::chrono::milliseconds lap() { return std::chrono::milliseconds{0}; }

private:
    typename Clock::time_point started_{};
};

// ---- 2. <random>: engines are portable, distributions are NOT --------------------------------------------------------------------------------------
// std::mt19937 produces the same sequence from the same seed on every compiler and platform (the standard specifies it). std::uniform_int_distribution
// does NOT: its algorithm is up to the library, so a "reproducible" simulation built on it differs between GCC, Clang and MSVC.
// So the mapping from random bits to a range is written here, and it must be UNBIASED: `engine() % n` favors small values unless n divides 2^32.
// TODO: uniform_below(rng, n) is a value in [0, n), n >= 1, each with the same probability. Draw 32-bit values; REJECT (draw again) those in the
// incomplete block at the top: r >= 2^32 - (2^32 % n).   (use std::uint64_t for 2^32)
template <class Engine>
std::uint32_t uniform_below(Engine& rng, std::uint32_t n) {
    (void)rng;
    (void)n;
    return 0;
}

// TODO: `count` rolls of a die with `sides` sides (1..sides), from a std::mt19937 seeded with `seed`: the same arguments always give the same rolls.
inline std::vector<int> simulate_rolls(std::uint32_t seed, int count, int sides) {
    (void)seed;
    (void)count;
    (void)sides;
    return {};
}

// TODO: a Fisher-Yates shuffle that is reproducible everywhere (std::shuffle is not, for the same reason as the distributions):
// for i from size-1 down to 1: j = uniform_below(rng, i + 1); swap(v[i], v[j]).
template <class Engine>
void shuffle_deterministic(std::vector<int>& values, Engine& rng) {
    (void)values;
    (void)rng;
}

// ---- 3. <numeric> ------------------------------------------------------------------------------------------------------------------------------------------
// TODO: the running totals:  prefix_sums({1, 2, 3, 4}) is {1, 3, 6, 10}. One allocation for the result: size the vector first and use std::inclusive_scan(first, last, out, std::plus<>(), 0LL): the running total has the type of
// the initial value (a long long), while std::partial_sum would add up in the type of the input (int) and overflow.
inline std::vector<long long> prefix_sums(const std::vector<int>& values) {
    (void)values;
    return {};
}

// TODO: the dot product of two vectors of the same length, accumulated in a long long (ints of 100000 times 100000 do not fit in an int).
// std::inner_product(a.begin(), a.end(), b.begin(), 0LL) has the wrong multiplication type: pass the two operations, or write a loop.
inline long long dot_product(const std::vector<int>& a, const std::vector<int>& b) {
    (void)a;
    (void)b;
    return 0;
}

// TODO: the greatest common divisor of all the values (0 for no values): gcd_of_all({12, 18, 30}) is 6.   std::gcd, std::accumulate
inline int gcd_of_all(const std::vector<int>& values) {
    (void)values;
    return -1;
}

// ---- 4. Floating point ---------------------------------------------------------------------------------------------------------------------------------------
// TODO: are a and b equal for practical purposes? |a - b| <= max(abs_tol, rel_tol * max(|a|, |b|)). Equal values (including two infinities) are nearly
// equal; a NaN is not nearly equal to anything, and an infinity is nearly equal only to the same infinity. `==` on doubles is almost never what you want: 0.1 + 0.2 != 0.3.
inline bool nearly_equal(double a, double b, double rel_tol = 1e-9, double abs_tol = 0.0) {
    (void)a;
    (void)b;
    (void)rel_tol;
    (void)abs_tol;
    return false;
}

// TODO: the sum of the values with Kahan (compensated) summation: it keeps the low-order bits that a plain `sum += x` throws away. A million additions
// of 0.1 with a plain loop are off by about 1e-6; this one is exact to about 1e-11.
//   double sum = 0, c = 0;  for (x : values) { y = x - c; t = sum + y; c = (t - sum) - y; sum = t; }
inline double sum_accurate(const std::vector<double>& values) {
    (void)values;
    return 0.0;
}
