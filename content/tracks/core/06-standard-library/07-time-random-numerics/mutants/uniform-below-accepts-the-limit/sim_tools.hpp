// MUTANT: r > limit.
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

// ---- 1. std::chrono: durations carry their UNIT in the type ------------------------------------------------------------------------------
// The sum of the three durations, in milliseconds. Mixed units convert automatically (and exactly) to the finer one: no *1000 anywhere.
inline std::chrono::milliseconds total_time(std::chrono::minutes m, std::chrono::seconds s, std::chrono::milliseconds ms) {
    return m + s + ms;
}

// The duration as a number of seconds with a fraction: to_fractional_seconds(1500ms) is 1.5. (std::chrono::duration<double> is "seconds as a double":
// converting to it never truncates. duration_cast to an integer duration TRUNCATES toward zero.)
inline double to_fractional_seconds(std::chrono::milliseconds d) {
    return std::chrono::duration<double>(d).count();
}

// "H h MM m SS s": hours (not limited to 24), minutes and seconds with two digits; the milliseconds are dropped (truncated).
//   pretty_duration(3723004ms) is "1h 02m 03s";  pretty_duration(0ms) is "0h 00m 00s";  a negative duration gets a '-' in front: "-0h 00m 01s".
inline std::string pretty_duration(std::chrono::milliseconds d) {
    const bool negative = d < std::chrono::milliseconds::zero();
    const std::chrono::milliseconds magnitude = negative ? -d : d;
    const auto hours = std::chrono::duration_cast<std::chrono::hours>(magnitude);
    const auto minutes = std::chrono::duration_cast<std::chrono::minutes>(magnitude - hours);
    const auto seconds = std::chrono::duration_cast<std::chrono::seconds>(magnitude - hours - minutes);
    return std::format("{}{}h {:02}m {:02}s", negative ? "-" : "", hours.count(), minutes.count(), seconds.count());
}

// A stopwatch that reads time from ANY clock type (std::chrono::steady_clock in a program, a fake clock in a test): `Clock::now()` is the only thing
// it needs. Injecting the clock is how time-dependent code becomes testable and deterministic.
template <class Clock>
class Stopwatch {
public:
    void start() { started_ = Clock::now(); }
    // Milliseconds since start() (truncated).
    std::chrono::milliseconds elapsed() const { return std::chrono::duration_cast<std::chrono::milliseconds>(Clock::now() - started_); }
    // Milliseconds since the previous lap() (or since start()), and the next lap begins now.
    std::chrono::milliseconds lap() {
        const auto now = Clock::now();
        const auto d = std::chrono::duration_cast<std::chrono::milliseconds>(now - started_);
        started_ = now;
        return d;
    }

private:
    typename Clock::time_point started_{};
};

// ---- 2. <random>: engines are portable, distributions are NOT --------------------------------------------------------------------------------------
// std::mt19937 produces the same sequence from the same seed on every compiler and platform (the standard specifies it). std::uniform_int_distribution
// does NOT: its algorithm is up to the library, so a "reproducible" simulation built on it differs between GCC, Clang and MSVC.
// So the mapping from random bits to a range is written here, and it must be UNBIASED: `engine() % n` favors small values unless n divides 2^32.
// uniform_below(rng, n) is a value in [0, n), n >= 1, each with the same probability. Draw 32-bit values; REJECT (draw again) those in the incomplete
// block at the top: r >= 2^32 - (2^32 % n).
template <class Engine>
std::uint32_t uniform_below(Engine& rng, std::uint32_t n) {
    const std::uint64_t range = std::uint64_t{1} << 32;
    const std::uint64_t limit = range - range % n;  // the largest multiple of n that fits
    std::uint64_t r = rng();
    while (r > limit) r = rng();
    return static_cast<std::uint32_t>(r % n);
}

// `count` rolls of a die with `sides` sides (1..sides), from a std::mt19937 seeded with `seed`: the same arguments always give the same rolls.
inline std::vector<int> simulate_rolls(std::uint32_t seed, int count, int sides) {
    std::mt19937 rng(seed);
    std::vector<int> rolls;
    rolls.reserve(static_cast<std::size_t>(count));
    for (int i = 0; i < count; ++i) rolls.push_back(static_cast<int>(uniform_below(rng, static_cast<std::uint32_t>(sides))) + 1);
    return rolls;
}

// A Fisher-Yates shuffle that is reproducible everywhere (std::shuffle is not, for the same reason as the distributions):
// for i from size-1 down to 1: j = uniform_below(rng, i + 1); swap(v[i], v[j]).
template <class Engine>
void shuffle_deterministic(std::vector<int>& values, Engine& rng) {
    for (std::size_t i = values.size(); i > 1; --i) {
        const std::size_t j = uniform_below(rng, static_cast<std::uint32_t>(i));
        std::swap(values[i - 1], values[j]);
    }
}

// ---- 3. <numeric> ------------------------------------------------------------------------------------------------------------------------------------------
// The running totals:  prefix_sums({1, 2, 3, 4}) is {1, 3, 6, 10}. One allocation for the result (std::inclusive_scan: like std::partial_sum, but the type of the running total is the type of the initial value, here long long).
inline std::vector<long long> prefix_sums(const std::vector<int>& values) {
    std::vector<long long> sums(values.size());
    std::inclusive_scan(values.begin(), values.end(), sums.begin(), std::plus<>(), 0LL);  // the running total is a long long
    return sums;
}

// The dot product of two vectors of the same length, accumulated in a long long (ints of 100000 times 100000 do not fit in an int).
inline long long dot_product(const std::vector<int>& a, const std::vector<int>& b) {
    return std::inner_product(a.begin(), a.end(), b.begin(), 0LL, std::plus<>(), [](int x, int y) { return static_cast<long long>(x) * y; });
}

// The greatest common divisor of all the values (0 for no values): gcd_of_all({12, 18, 30}) is 6.
inline int gcd_of_all(const std::vector<int>& values) {
    return std::accumulate(values.begin(), values.end(), 0, [](int g, int x) { return std::gcd(g, x); });
}

// ---- 4. Floating point ---------------------------------------------------------------------------------------------------------------------------------------
// Are a and b equal for practical purposes? |a - b| <= max(abs_tol, rel_tol * max(|a|, |b|)). Equal values (including two infinities) are nearly equal;
// a NaN is not nearly equal to anything. `==` on doubles is almost never what you want: 0.1 + 0.2 != 0.3.
inline bool nearly_equal(double a, double b, double rel_tol = 1e-9, double abs_tol = 0.0) {
    if (a == b) return true;  // also infinities of the same sign
    if (std::isnan(a) || std::isnan(b)) return false;
    if (std::isinf(a) || std::isinf(b)) return false;  // different infinities, or an infinity and a number: |a - b| would be infinite and "<= rel * inf" would say yes
    return std::fabs(a - b) <= std::max(abs_tol, rel_tol * std::max(std::fabs(a), std::fabs(b)));
}

// The sum of the values with Kahan (compensated) summation: it keeps the low-order bits that a plain `sum += x` throws away. A million additions
// of 0.1 with a plain loop are off by about 1e-6; this one is exact to about 1e-11.
inline double sum_accurate(const std::vector<double>& values) {
    double sum = 0.0;
    double compensation = 0.0;  // what was lost in the previous addition
    for (double x : values) {
        const double y = x - compensation;
        const double t = sum + y;
        compensation = (t - sum) - y;
        sum = t;
    }
    return sum;
}
