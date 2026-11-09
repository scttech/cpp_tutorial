# Time, randomness and numerics

Three areas where code that looks right is quietly wrong: **time** (units, clocks that jump), **randomness** (bias, and results that differ between compilers), and **numbers** (overflow, floating-point). The standard library has good tools for each, and this lesson is about using them so that the results are correct *and reproducible*.

## `<chrono>`: the unit is in the type

```cpp
using namespace std::chrono_literals;
auto timeout = 1min + 30s + 250ms;           // a std::chrono::milliseconds: mixed units convert exactly to the finer one
double s = std::chrono::duration<double>(1500ms).count();                 // 1.5: a double-based duration never truncates
auto whole = std::chrono::duration_cast<std::chrono::seconds>(1500ms);    // 1s: duration_cast TRUNCATES toward zero
```

No `* 1000` and no "is this in seconds or milliseconds?": passing `std::chrono::seconds` where milliseconds are expected still compiles (it converts exactly), but passing `milliseconds` where `seconds` are expected does **not** (it would lose information) unless you `duration_cast`.

- **`steady_clock`** only goes forward: use it to measure elapsed time. **`system_clock`** is wall-clock time and can jump (NTP, daylight saving): use it for dates.
- **Inject the clock** (`template <class Clock> class Stopwatch`) and a test can supply a fake clock that it advances by hand: time-dependent code becomes deterministic. That is the whole trick, and it is how `Stopwatch` is tested here.

## `<random>`: engines are portable, distributions are not

`std::rand()` is a poor generator (small range, low-quality low bits, global state). Use an **engine** (`std::mt19937`) seeded explicitly: the standard fixes its algorithm, so **the same seed gives the same numbers on every compiler and platform**. That is what makes a simulation reproducible, and a failing run debuggable.

But **`std::uniform_int_distribution` and `std::shuffle` are *not* specified**: libstdc++, libc++ and MSVC produce different values from the same engine. So for reproducible results across platforms, map random bits to a range yourself, and do it **without bias**:

```cpp
engine() % n          // BIASED: 2^32 is not a multiple of n, so small remainders come up more often
```

The fix is **rejection sampling**: the values `[0, 2^32)` split into whole blocks of `n` and one incomplete block at the top. Throw away (redraw) a value that falls in the incomplete block, and `r % n` is exactly uniform. A **Fisher-Yates shuffle** built on that (`for i = size-1 down to 1: swap(v[i], v[uniform_below(i + 1)])`) is a uniformly random permutation, identical everywhere.

Never seed from the clock for a test or a simulation you want to replay: seed with a number you can print.

## `<numeric>` and integer overflow

`std::accumulate`, `std::partial_sum`, `std::inner_product` and friends compute in **the type of the initial value**. `std::accumulate(v.begin(), v.end(), 0)` adds in `int` even if the elements are `long long`: it overflows silently, and `0LL` fixes it. `std::inclusive_scan(first, last, out, std::plus<>(), 0LL)` is the running-total version that takes the same initial value. `std::gcd` and `std::lcm` (C++17) work on any integer type.

## Floating point

- `0.1 + 0.2 == 0.3` is **false**. Compare with a tolerance: **relative** (`|a - b| <= rel_tol * max(|a|, |b|)`) for large values, plus an **absolute** one for values near zero; handle exactly equal values first (two infinities) and NaN (never equal to anything, itself included).
- Adding many numbers of different sizes loses the low-order bits of each addition. **Kahan summation** carries the lost part in a compensation variable: a million additions of `0.1` give 100000 to about 1e-11 instead of being off by about 1e-6. (Compilers must not be told `-ffast-math`: it may optimise the compensation away.)

## Your task

Fill in `sim_tools.hpp`: `total_time`, `to_fractional_seconds`, `pretty_duration`, `Stopwatch<Clock>`, `uniform_below`, `simulate_rolls`, `shuffle_deterministic`, `prefix_sums`, `dot_product`, `gcd_of_all`, `nearly_equal`, `sum_accurate`. The tiers: **Works**, **Robust** (the rejection boundary, the standard mt19937 sequence, overflow, infinities and NaN, 100+ hours, a negative duration), **Fast** (one allocation for the result, nothing allocated by the numeric loops, a million shuffles and rolls).
