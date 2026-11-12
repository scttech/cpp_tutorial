// MUTANT: the last grid point may not be INT64_MAX
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

namespace detail {
// b - a for a <= b, exact even when it does not fit in int64
inline std::uint64_t diff_u(Time a, Time b) { return static_cast<std::uint64_t>(b) - static_cast<std::uint64_t>(a); }
// the offset of start from INT64_MIN, i.e. start mapped to 0 .. 2^64-1 in order
inline std::uint64_t offset(Time start) { return static_cast<std::uint64_t>(start) ^ (std::uint64_t{1} << 63); }
inline bool fits(Time start, std::uint64_t distance) { return distance <= std::numeric_limits<std::uint64_t>::max() - offset(start); }
}  // namespace detail

inline std::vector<Sample> normalize(std::vector<Sample> raw) {
    raw.erase(std::remove_if(raw.begin(), raw.end(), [](const Sample& s) { return !std::isfinite(s.v); }), raw.end());
    std::stable_sort(raw.begin(), raw.end(), [](const Sample& a, const Sample& b) { return a.t < b.t; });
    std::size_t out = 0;
    for (std::size_t i = 0; i < raw.size(); ++i) {
        if (out > 0 && raw[out - 1].t == raw[i].t) raw[out - 1] = raw[i];   // a repeated timestamp: the last one wins
        else raw[out++] = raw[i];
    }
    raw.resize(out);
    return raw;
}

inline std::optional<Time> align_down(Time t, Time step) {
    if (step <= 0) return std::nullopt;
    const Time r = t % step;   // has the sign of t
    if (r == 0) return t;
    if (r > 0) return t - r;
    const Time back = r + step;   // r < 0: the multiple below t is t - (step + r)
    if (t < std::numeric_limits<Time>::min() + back) return std::nullopt;
    return t - back;
}

inline std::optional<Time> align_up(Time t, Time step) {
    if (step <= 0) return std::nullopt;
    const Time r = t % step;
    if (r == 0) return t;
    if (r < 0) return t - r;   // towards zero: always representable
    const Time forward = step - r;
    if (t > std::numeric_limits<Time>::max() - forward) return std::nullopt;
    return t + forward;
}

inline std::optional<std::uint64_t> bucket_of(Time t, Time start, Time width) {
    if (width <= 0 || t < start) return std::nullopt;
    return detail::diff_u(start, t) / static_cast<std::uint64_t>(width);
}

inline std::optional<std::vector<double>> resample(std::span<const Sample> s, Time start, Time step, std::size_t count, Fill fill, Time max_gap = kNoLimit) {
    if (step <= 0 || count > kMaxPoints || max_gap < 0) return std::nullopt;
    const std::uint64_t last_offset = count == 0 ? 0 : static_cast<std::uint64_t>(count - 1) * static_cast<std::uint64_t>(step);
    if (count > 1 && last_offset / static_cast<std::uint64_t>(step) != count - 1) return std::nullopt;
    if (!detail::fits(start, last_offset) || detail::offset(start) + last_offset == std::numeric_limits<std::uint64_t>::max()) return std::nullopt;
    std::vector<double> out(count, std::numeric_limits<double>::quiet_NaN());
    std::size_t j = 0;   // samples [0, j) are at or before the grid time
    const std::uint64_t gap = static_cast<std::uint64_t>(max_gap);
    const bool unlimited = max_gap == kNoLimit;
    const auto within = [&](std::uint64_t d) { return unlimited || d <= gap; };
    for (std::size_t k = 0; k < count; ++k) {
        const Time g = static_cast<Time>(static_cast<std::uint64_t>(start) + static_cast<std::uint64_t>(k) * static_cast<std::uint64_t>(step));
        while (j < s.size() && s[j].t <= g) ++j;
        if (j > 0 && s[j - 1].t == g) {
            out[k] = s[j - 1].v;
            continue;
        }
        if (fill == Fill::None || j == 0) continue;
        const Sample& prev = s[j - 1];
        if (fill == Fill::Previous) {
            if (within(detail::diff_u(prev.t, g))) out[k] = prev.v;
        } else if (j < s.size()) {
            const Sample& next = s[j];
            const std::uint64_t whole = detail::diff_u(prev.t, next.t);
            if (within(whole)) {
                const double f = static_cast<double>(detail::diff_u(prev.t, g)) / static_cast<double>(whole);
                out[k] = prev.v * (1.0 - f) + next.v * f;
            }
        }
    }
    return out;
}

inline std::optional<std::vector<Bucket>> aggregate(std::span<const Sample> s, Time start, Time width, std::size_t count) {
    if (width <= 0 || count > kMaxBuckets) return std::nullopt;
    const std::uint64_t total = static_cast<std::uint64_t>(count) * static_cast<std::uint64_t>(width);   // count < 2^20, so this overflows only for huge widths
    if (count > 0 && total / static_cast<std::uint64_t>(count) != static_cast<std::uint64_t>(width)) return std::nullopt;
    if (!detail::fits(start, total)) return std::nullopt;
    std::vector<Bucket> out(count);
    for (const Sample& x : s) {
        const auto idx = bucket_of(x.t, start, width);
        if (!idx || *idx >= count) continue;
        Bucket& b = out[static_cast<std::size_t>(*idx)];
        if (b.n == 0) b.first = b.min = b.max = x.v;
        b.min = std::min(b.min, x.v);
        b.max = std::max(b.max, x.v);
        b.last = x.v;
        b.sum += x.v;
        ++b.n;
    }
    return out;
}

}  // namespace ts
