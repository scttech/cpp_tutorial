#pragma once
// YOUR TIME-SERIES RESAMPLING.
//
// ts::Time is std::int64_t (milliseconds, or any unit; negative times are fine). A ts::Sample is { Time t; double v; }.
//
// ts::normalize(raw) -> std::vector<Sample>
//     the samples sorted by time, with NaN/infinite values dropped, and ONE sample per timestamp: when a timestamp repeats, the LAST one in the input wins.
//     Everything below expects a normalised series (strictly increasing times); it is not re-checked.
//
// ts::align_down(t, step) -> optional<Time>      the largest multiple of step that is <= t, i.e. FLOOR (align_down(-1, 1000) is -1000, not 0). nullopt if step <= 0
//                                                or the answer is below INT64_MIN.
// ts::align_up(t, step)   -> optional<Time>      the smallest multiple of step that is >= t. nullopt if step <= 0 or the answer is above INT64_MAX.
// ts::bucket_of(t, start, width) -> optional<uint64_t>    the index of the half-open bucket [start + k * width, start + (k + 1) * width) holding t; nullopt if t < start or width <= 0.
//                                                Must be right for t - start larger than INT64_MAX (e.g. start = INT64_MIN, t = INT64_MAX): do the subtraction in unsigned arithmetic.
//
// ts::Fill { None, Previous, Linear }
// ts::resample(s, start, step, count, fill, max_gap = kNoLimit) -> optional<vector<double>>
//     the value of the series at the grid times  start, start + step, ..., start + (count - 1) * step.  A grid time that cannot be filled is NaN ("missing"), never 0.
//       * a sample exactly at the grid time: its value, whatever the fill and whatever max_gap (the series itself is never discarded);
//       * Fill::None: otherwise missing;
//       * Fill::Previous: the value of the latest sample before the grid time, if the grid time is at most max_gap after it (<=); before the first sample: missing;
//       * Fill::Linear: needs a sample on each side. If the two are more than max_gap apart (<=) the point is missing. Value = v0 * (1 - f) + v1 * f, f = (g - t0) / (t1 - t0).
//         NO extrapolation: before the first and after the last sample: missing.
//     max_gap == kNoLimit means NO limit at all, also for differences larger than INT64_MAX.
//     nullopt if step <= 0, count > kMaxPoints, max_gap < 0, or the grid would run past INT64_MAX. A grid whose last point is exactly INT64_MAX is fine.
//     Time differences can exceed INT64_MAX (t0 = INT64_MIN, t1 = INT64_MAX): compute them in unsigned arithmetic. One pass over samples and grid together: O(n + count).
//
// ts::Bucket { size_t n; double sum, min, max, first, last; mean() }    an empty bucket has n = 0, sum = 0, mean() NaN and min/max/first/last NaN.
// ts::aggregate(s, start, width, count) -> optional<vector<Bucket>>
//     count buckets [start + k * width, start + (k + 1) * width). A sample outside all of them is ignored; "first" and "last" are by time. nullopt if width <= 0, count > kMaxBuckets,
//     or the end of the last bucket (start + count * width) would pass INT64_MAX.
#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <optional>
#include <span>
#include <vector>

namespace ts {

using Time = std::int64_t;
inline constexpr Time kNoLimit = std::numeric_limits<Time>::max();
inline constexpr std::size_t kMaxPoints = std::size_t{1} << 24;
inline constexpr std::size_t kMaxBuckets = std::size_t{1} << 20;

struct Sample {
    Time t;
    double v;
};

enum class Fill { None, Previous, Linear };

struct Bucket {
    std::size_t n = 0;
    double sum = 0;
    double min = std::numeric_limits<double>::quiet_NaN();
    double max = std::numeric_limits<double>::quiet_NaN();
    double first = std::numeric_limits<double>::quiet_NaN();
    double last = std::numeric_limits<double>::quiet_NaN();
    double mean() const { return n ? sum / static_cast<double>(n) : std::numeric_limits<double>::quiet_NaN(); }
};

// TODO: everything below returns a placeholder

inline std::vector<Sample> normalize(std::vector<Sample> raw) { return raw; }

inline std::optional<Time> align_down(Time t, Time) { return t; }
inline std::optional<Time> align_up(Time t, Time) { return t; }
inline std::optional<std::uint64_t> bucket_of(Time, Time, Time) { return std::nullopt; }

inline std::optional<std::vector<double>> resample(std::span<const Sample>, Time, Time, std::size_t, Fill, Time = kNoLimit) { return std::nullopt; }
inline std::optional<std::vector<Bucket>> aggregate(std::span<const Sample>, Time, Time, std::size_t) { return std::nullopt; }

}  // namespace ts
