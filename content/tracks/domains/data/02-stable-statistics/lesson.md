# Numerically Stable Statistics

> **You're on the data platform team.** A dashboard shows the standard deviation of request latencies, and today it says `-3.2e-5 ms`. Another job computes the mean of a billion sensor readings and is off in the fourth decimal place; a third merges per-shard summaries and gets different answers depending on how the shards were grouped. All three are the same story: the textbook formulas are *algebraically* right and *numerically* wrong. This challenge is the toolbox the pros reach for.

## Catastrophic cancellation

The textbook variance, `(sum(x^2) - sum(x)^2 / n) / (n - 1)`, subtracts two huge, nearly equal numbers. A `double` has about 16 significant digits. For readings like `1e9 + 4, 1e9 + 7, 1e9 + 13, 1e9 + 16` the two terms are about `4e18` and agree in their first 18 digits, so the difference (the answer, `90`) is *rounding noise*: it can come out zero, or negative. The standard deviation of a negative number is `NaN`.

**Welford's algorithm** never forms the big numbers. It keeps the running mean and the running sum of squared deviations from the *current* mean:

```
n += 1
delta = x - mean
mean += delta / n
m2   += delta * (x - mean)      // note: the NEW mean in the second factor
variance = m2 / (n - 1)
```

Every term is a small number, and `m2` is a sum of non-negative terms, so the variance can never go negative.

## Mergeable summaries

Distributed systems compute a summary per shard and combine them. Welford's state `(n, mean, m2)` merges exactly (Chan, Golub and LeVeque, 1979):

```
delta = mean_b - mean_a
mean  = mean_a + delta * n_b / n
m2    = m2_a + m2_b + delta^2 * n_a * n_b / n
```

The last term is the extra variance caused by the two groups having *different means*; leave it out and the merged variance is too small whenever the shards differ. Merging with an empty summary must be a no-op (no `0 / 0`).

## Summation: Kahan and Neumaier

Adding a million `0.1`s gives `100000.00000133...`: every addition rounds, and the errors drift in one direction. Compensated summation keeps a second variable with the rounding error of each addition and puts it back at the end. **Kahan's** original loses the error when the new addend is larger than the running sum (`1 + 1e100 + 1 - 1e100` gives `0` instead of `2`); **Neumaier's** variant tests which of the two is bigger and recovers the lost bits from the right one.

## Correlation and its traps

`r = cov(x, y) / (sd(x) * sd(y))`, computed in the same online way. Two traps: (1) rounding can push a perfect correlation to `1.0000000000000002`, which makes `acos(r)` `NaN` downstream, so clamp to `[-1, 1]`; (2) `m2x * m2y` can overflow even when each fits (`1e300 * 1e300`); take the square roots first.

## Percentiles

Sorting everything for one percentile is `O(n log n)`; `std::nth_element` finds the k-th smallest in `O(n)`. With the linear-interpolation definition you need ranks `lo` and `lo + 1`: select `lo`, then the smallest element of the upper partition. Interpolate as `a * (1 - f) + b * f`; the "obvious" `a + f * (b - a)` overflows for `-1e308` and `1e308`. NaN has no place in an ordering and would make `nth_element` undefined, so refuse it.

## The tiers

- **Works**: small samples, the empty and one-element cases, NaN/infinity policy, compensated sums, covariance, correlation, percentile values and refusals.
- **Robust**: offsets of 1e9 to 1e12; never-negative variance; 200 random merge splits (with empty parts); the unequal-weights merge; ill-conditioned sums against a long-double reference; correlation of 1e150 values; percentile extremes against a sorted reference.
- **Fast**: nothing allocates in the streaming path; percentile makes one copy; a million values and 40 percentiles of a million.
- **Clean**: no raw memory or exceptions; `nth_element` not `sort`; no `pow`, `accumulate` or `reduce`.
