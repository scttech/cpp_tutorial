# Time-Series Resampling

> **You're on the data team at a monitoring company.** Devices report whenever they feel like it: every 10 s, every 7 s, twice in the same millisecond, five minutes late and out of order. A dashboard wants one point per minute; a model wants a perfect grid; a billing job wants hourly totals. Resampling is the bridge, and it is full of small decisions that quietly corrupt data when made wrongly: what do you show for a minute with no report? Is a reading at exactly 12:00:00 in the 11:00 hour or the 12:00 hour? What does "an hour earlier than the epoch" round to?

## Clean the input once

`normalize` sorts (stably), drops NaN and infinite readings (they would poison every average they touch), and applies a **duplicate policy**: here the last report of a repeated timestamp wins. Everything after it can assume strictly increasing times, which is what makes a single linear pass possible.

## Filling a grid point

For a grid time `g` there are three honest answers, and "0" is never one of them:

| Fill | Value at `g` | When it is *missing* (NaN) |
|---|---|---|
| `None` | only a sample exactly at `g` | otherwise |
| `Previous` ("last observation carried forward") | the latest sample at or before `g` | before the first sample; or more than `max_gap` after it |
| `Linear` | `v0 * (1 - f) + v1 * f`, `f = (g - t0) / (t1 - t0)` | before the first or after the last sample (**no extrapolation**); or the neighbours are more than `max_gap` apart |

A device that went silent for an hour should produce a hole in the chart, not a flat line that says "everything is fine". That is what `max_gap` is for. An exact hit is always reported: the series itself is never thrown away.

Because both the grid and the samples are sorted, one **pointer into the samples** that only moves forward is enough: `O(n + count)` instead of `O(n x count)` (or `O(count log n)` with a binary search per point).

## Time arithmetic is where the bugs are

- **Floor, not truncate.** `t / step * step` rounds toward zero, so for `t = -1` and `step = 1000` it gives `0`: the reading from 1 ms before the epoch is filed in the wrong second. Use the remainder and adjust when it is negative. The same applies to bucket indices.
- **Differences overflow.** `t1 - t0` for `INT64_MIN` and `INT64_MAX` is `2^64 - 1`, which is not an `int64_t`; signed overflow is undefined behaviour. For `a <= b`, `uint64_t(b) - uint64_t(a)` is always exact. Real data has these: `0`, `-1` and `INT64_MAX` sentinels are everywhere.
- **The grid can run off the end.** `start + k * step` can pass `INT64_MAX` (and `k * step` can wrap long before). Check the whole span up front, in unsigned arithmetic, and return "no" instead of a garbage grid. A grid ending exactly at `INT64_MAX` is fine.
- **Buckets are half-open**: `[start + k * width, start + (k + 1) * width)`. A reading on a boundary belongs to the *next* bucket. Everything else (first, last, min, max, mean) follows from that single rule.

## The tiers

- **Works**: normalisation, floor/ceiling alignment, buckets, the three fills, `max_gap` inclusive rules, aggregates.
- **Robust**: alignment checked against its definition for every sign; the extremes of `int64`; 500 random series against a brute-force 128-bit reference; differences wider than `int64`; the grid ending at `INT64_MAX`; negative times; random aggregates.
- **Fast**: one allocation per result; a million samples onto a two-million-point grid; sorting a million samples.
- **Clean**: no raw memory or exceptions; no floating-point `floor`/`fmod`/`round`/`pow` on timestamps; no per-point searching.
