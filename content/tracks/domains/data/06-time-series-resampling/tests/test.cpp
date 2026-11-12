#define ZTH_TRACK_ALLOCATIONS
#include <cmath>
#include <limits>
#include <vector>

#include "timeseries.hpp"
#include "zth_test.hpp"

using ts::Bucket;
using ts::Fill;
using ts::Sample;
using ts::Time;
using Vec = std::vector<double>;

namespace {

const double kNan = std::numeric_limits<double>::quiet_NaN();
constexpr Time kMin = std::numeric_limits<Time>::min();
constexpr Time kMax = std::numeric_limits<Time>::max();

bool same(const Vec& a, const Vec& b, double eps = 1e-12) {
    if (a.size() != b.size()) return false;
    for (std::size_t i = 0; i < a.size(); ++i) {
        if (std::isnan(a[i]) != std::isnan(b[i])) return false;
        if (!std::isnan(a[i]) && !(std::fabs(a[i] - b[i]) <= eps)) return false;
    }
    return true;
}
Vec grid(const std::vector<Sample>& s, Time start, Time step, std::size_t count, Fill fill, Time gap = ts::kNoLimit) {
    const auto r = ts::resample(s, start, step, count, fill, gap);
    return r ? *r : Vec{12345.0};   // a sentinel that never equals an expected vector
}
const std::vector<Sample> kS = {{0, 10}, {1000, 20}, {4000, 0}, {5000, 50}};

// brute force with 128-bit integers: scan every sample for every grid point
Vec ref_resample(const std::vector<Sample>& s, Time start, Time step, std::size_t count, Fill fill, Time max_gap) {
    Vec out(count, kNan);
    for (std::size_t k = 0; k < count; ++k) {
        const __int128 g = static_cast<__int128>(start) + static_cast<__int128>(k) * step;
        const Sample* prev = nullptr;
        const Sample* next = nullptr;
        for (const Sample& x : s) {
            if (x.t <= g) prev = &x;
            else if (!next) next = &x;
        }
        if (prev && prev->t == g) {
            out[k] = prev->v;
            continue;
        }
        if (fill == Fill::None || !prev) continue;
        if (fill == Fill::Previous) {
            if (max_gap == ts::kNoLimit || g - prev->t <= max_gap) out[k] = prev->v;
        } else if (next && (max_gap == ts::kNoLimit || static_cast<__int128>(next->t) - prev->t <= max_gap)) {
            const double f = static_cast<double>(g - prev->t) / static_cast<double>(static_cast<__int128>(next->t) - prev->t);
            out[k] = prev->v * (1.0 - f) + next->v * f;
        }
    }
    return out;
}

}  // namespace

// ================================================ Works =======================================================
ZTH_TEST("works", "normalize sorts, drops NaN and infinity, and keeps the last sample of a repeated timestamp") {
    const std::vector<Sample> raw = {{5, 1}, {3, 2}, {5, 3}, {4, kNan}, {1, std::numeric_limits<double>::infinity()}, {0, 0}, {3, -std::numeric_limits<double>::infinity()}, {8, 9.0}, {8, 4.0}};
    const std::vector<Sample> s = ts::normalize(raw);
    ZTH_EXPECT_EQ(s.size(), std::size_t{4});
    ZTH_EXPECT_TRUE(s[3].t == 8 && s[3].v == 4);   // 9 came first, 4 last
    ZTH_EXPECT_TRUE(s[0].t == 0 && s[0].v == 0);
    ZTH_EXPECT_TRUE(s[1].t == 3 && s[1].v == 2);   // the -inf at t = 3 was dropped, so the 2 remains
    ZTH_EXPECT_TRUE(s[2].t == 5 && s[2].v == 3);   // the later of the two samples at t = 5
    ZTH_EXPECT_TRUE(ts::normalize({}).empty());
    const std::vector<Sample> triple = ts::normalize({{7, 1}, {7, 2}, {7, 3}});
    ZTH_EXPECT_TRUE(triple.size() == 1 && triple[0].v == 3);
}
ZTH_TEST("works", "align_down is a floor and align_up a ceiling, also for negative times") {
    ZTH_EXPECT_EQ(ts::align_down(1500, 1000).value_or(-1), Time{1000});
    ZTH_EXPECT_EQ(ts::align_down(999, 1000).value_or(-1), Time{0});
    ZTH_EXPECT_EQ(ts::align_down(0, 1000).value_or(-1), Time{0});
    ZTH_EXPECT_EQ(ts::align_down(1000, 1000).value_or(-1), Time{1000});
    ZTH_EXPECT_EQ(ts::align_down(-1, 1000).value_or(1), Time{-1000});   // not 0
    ZTH_EXPECT_EQ(ts::align_down(-1000, 1000).value_or(1), Time{-1000});
    ZTH_EXPECT_EQ(ts::align_down(-1001, 1000).value_or(1), Time{-2000});
    ZTH_EXPECT_EQ(ts::align_up(1500, 1000).value_or(-1), Time{2000});
    ZTH_EXPECT_EQ(ts::align_up(1000, 1000).value_or(-1), Time{1000});
    ZTH_EXPECT_EQ(ts::align_up(1, 1000).value_or(-1), Time{1000});
    ZTH_EXPECT_EQ(ts::align_up(-1, 1000).value_or(1), Time{0});
    ZTH_EXPECT_EQ(ts::align_up(-1500, 1000).value_or(1), Time{-1000});
    ZTH_EXPECT_EQ(ts::align_up(-1000, 1000).value_or(1), Time{-1000});
    ZTH_EXPECT_FALSE(ts::align_down(5, 0).has_value());
    ZTH_EXPECT_FALSE(ts::align_down(5, -10).has_value());
    ZTH_EXPECT_FALSE(ts::align_up(5, 0).has_value());
    ZTH_EXPECT_EQ(ts::align_down(7, 1).value_or(-1), Time{7});
    ZTH_EXPECT_EQ(ts::align_up(-7, 1).value_or(1), Time{-7});
}
ZTH_TEST("works", "bucket_of: half-open buckets, nothing before the start") {
    ZTH_EXPECT_EQ(ts::bucket_of(0, 0, 1000).value_or(99), std::uint64_t{0});
    ZTH_EXPECT_EQ(ts::bucket_of(999, 0, 1000).value_or(99), std::uint64_t{0});
    ZTH_EXPECT_EQ(ts::bucket_of(1000, 0, 1000).value_or(99), std::uint64_t{1});
    ZTH_EXPECT_EQ(ts::bucket_of(2500, 0, 1000).value_or(99), std::uint64_t{2});
    ZTH_EXPECT_EQ(ts::bucket_of(-500, -1000, 1000).value_or(99), std::uint64_t{0});
    ZTH_EXPECT_EQ(ts::bucket_of(500, -1000, 1000).value_or(99), std::uint64_t{1});
    ZTH_EXPECT_FALSE(ts::bucket_of(-1, 0, 1000).has_value());
    ZTH_EXPECT_FALSE(ts::bucket_of(5, 0, 0).has_value());
    ZTH_EXPECT_FALSE(ts::bucket_of(5, 0, -3).has_value());
}
ZTH_TEST("works", "resample with Fill::None gives only the exact matches") {
    ZTH_EXPECT_TRUE(same(grid(kS, 0, 1000, 7, Fill::None), Vec{10, 20, kNan, kNan, 0, 50, kNan}));
    ZTH_EXPECT_TRUE(same(grid(kS, 500, 1000, 5, Fill::None), Vec{kNan, kNan, kNan, kNan, kNan}));
}
ZTH_TEST("works", "resample with Fill::Previous carries a value forward, even past the last sample, never backwards") {
    ZTH_EXPECT_TRUE(same(grid(kS, 0, 1000, 7, Fill::Previous), Vec{10, 20, 20, 20, 0, 50, 50}));
    ZTH_EXPECT_TRUE(same(grid(kS, 500, 1000, 5, Fill::Previous), Vec{10, 20, 20, 20, 0}));
    ZTH_EXPECT_TRUE(same(grid(kS, -2000, 1000, 4, Fill::Previous), Vec{kNan, kNan, 10, 20}));   // nothing before the first sample
}
ZTH_TEST("works", "resample with Fill::Linear interpolates between neighbours and never extrapolates") {
    ZTH_EXPECT_TRUE(same(grid(kS, 0, 1000, 7, Fill::Linear), Vec{10, 20, 40.0 / 3.0, 20.0 / 3.0, 0, 50, kNan}));
    ZTH_EXPECT_TRUE(same(grid(kS, 500, 1000, 5, Fill::Linear), Vec{15, 20.0 * 5.0 / 6.0, 10, 20.0 / 6.0, 25}));
    ZTH_EXPECT_TRUE(same(grid(kS, -1000, 1000, 3, Fill::Linear), Vec{kNan, 10, 20}));
}
ZTH_TEST("works", "max_gap: a value is carried or interpolated only across gaps of at most that size (inclusive); exact matches always count") {
    ZTH_EXPECT_TRUE(same(grid(kS, 0, 1000, 7, Fill::Previous, 1000), Vec{10, 20, 20, kNan, 0, 50, 50}));
    ZTH_EXPECT_TRUE(same(grid(kS, 0, 1000, 7, Fill::Previous, 999), Vec{10, 20, kNan, kNan, 0, 50, kNan}));
    ZTH_EXPECT_TRUE(same(grid(kS, 0, 1000, 7, Fill::Previous, 0), Vec{10, 20, kNan, kNan, 0, 50, kNan}));
    ZTH_EXPECT_TRUE(same(grid(kS, 0, 1000, 7, Fill::Linear, 2000), Vec{10, 20, kNan, kNan, 0, 50, kNan}));   // 1000..4000 are 3000 apart
    ZTH_EXPECT_TRUE(same(grid(kS, 0, 1000, 7, Fill::Linear, 3000), Vec{10, 20, 40.0 / 3.0, 20.0 / 3.0, 0, 50, kNan}));   // exactly 3000: allowed
    ZTH_EXPECT_TRUE(same(grid(kS, 0, 1000, 7, Fill::Linear, 0), Vec{10, 20, kNan, kNan, 0, 50, kNan}));
    ZTH_EXPECT_FALSE(ts::resample(kS, 0, 1000, 3, Fill::Linear, -1).has_value());
}
ZTH_TEST("works", "resample: empty input, empty grid, one sample, and refused arguments") {
    ZTH_EXPECT_TRUE(same(grid({}, 0, 10, 3, Fill::Linear), Vec{kNan, kNan, kNan}));
    ZTH_EXPECT_TRUE(grid(kS, 0, 10, 0, Fill::Linear).empty());
    const std::vector<Sample> one = {{50, 7}};
    ZTH_EXPECT_TRUE(same(grid(one, 0, 25, 5, Fill::Linear), Vec{kNan, kNan, 7, kNan, kNan}));
    ZTH_EXPECT_TRUE(same(grid(one, 0, 25, 5, Fill::Previous), Vec{kNan, kNan, 7, 7, 7}));
    ZTH_EXPECT_FALSE(ts::resample(kS, 0, 0, 3, Fill::None).has_value());
    ZTH_EXPECT_FALSE(ts::resample(kS, 0, -5, 3, Fill::None).has_value());
    ZTH_EXPECT_FALSE(ts::resample(kS, 0, 1, ts::kMaxPoints + 1, Fill::None).has_value());
}
ZTH_TEST("works", "aggregate: counts, sums, min, max, first, last and mean per half-open bucket; empty buckets are marked") {
    const auto r = ts::aggregate(kS, 0, 2000, 3);
    ZTH_EXPECT_TRUE(r.has_value() && r->size() == 3);
    const Bucket& b0 = (*r)[0];
    ZTH_EXPECT_TRUE(b0.n == 2 && b0.sum == 30 && b0.min == 10 && b0.max == 20 && b0.first == 10 && b0.last == 20);
    ZTH_EXPECT_NEAR(b0.mean(), 15.0, 0);
    const Bucket& b1 = (*r)[1];
    ZTH_EXPECT_TRUE(b1.n == 0 && b1.sum == 0 && std::isnan(b1.min) && std::isnan(b1.max) && std::isnan(b1.first) && std::isnan(b1.last) && std::isnan(b1.mean()));
    const Bucket& b2 = (*r)[2];
    ZTH_EXPECT_TRUE(b2.n == 2 && b2.min == 0 && b2.max == 50 && b2.first == 0 && b2.last == 50);
    ZTH_EXPECT_NEAR(b2.mean(), 25.0, 0);
}
ZTH_TEST("works", "aggregate: samples outside the buckets are ignored, a sample on a boundary starts the next bucket, arguments are checked") {
    const std::vector<Sample> s = {{-5, 100}, {0, 1}, {9, 2}, {10, 3}, {19, 4}, {20, 5}, {99, 6}};
    const auto r = ts::aggregate(s, 0, 10, 2);
    ZTH_EXPECT_TRUE(r.has_value() && r->size() == 2);
    ZTH_EXPECT_TRUE((*r)[0].n == 2 && (*r)[0].sum == 3);   // 0 and 9
    ZTH_EXPECT_TRUE((*r)[1].n == 2 && (*r)[1].sum == 7);   // 10 and 19; 20 is outside
    ZTH_EXPECT_FALSE(ts::aggregate(s, 0, 0, 2).has_value());
    ZTH_EXPECT_FALSE(ts::aggregate(s, 0, -1, 2).has_value());
    ZTH_EXPECT_FALSE(ts::aggregate(s, 0, 1, ts::kMaxBuckets + 1).has_value());
    const auto none = ts::aggregate(s, 0, 10, 0);
    ZTH_EXPECT_TRUE(none.has_value() && none->empty());
}

// ================================================ Robust ======================================================
ZTH_TEST("robust", "alignment is a floor for every sign, checked against the definition on a grid of values and steps") {
    for (Time step : {Time{1}, Time{2}, Time{7}, Time{1000}, Time{86400000}})
        for (Time t = -3 * step - 1; t <= 3 * step + 1; ++t) {
            const auto d = ts::align_down(t, step);
            const auto u = ts::align_up(t, step);
            ZTH_EXPECT_TRUE(d.has_value() && u.has_value());
            ZTH_EXPECT_TRUE(*d % step == 0 && *d <= t && t - *d < step);
            ZTH_EXPECT_TRUE(*u % step == 0 && *u >= t && *u - t < step);
            if (step > 1000) t += step / 3;   // keep the loop short for the big step
        }
}
ZTH_TEST("robust", "alignment at the ends of the int64 range: refuse what cannot be represented, accept the rest") {
    ZTH_EXPECT_FALSE(ts::align_down(kMin, 1000).has_value());   // kMin is not a multiple of 1000
    ZTH_EXPECT_EQ(ts::align_down(kMin, 1).value_or(0), kMin);
    ZTH_EXPECT_EQ(ts::align_down(kMin, 2).value_or(0), kMin);   // kMin is even
    ZTH_EXPECT_EQ(ts::align_down(kMin + 1, 2).value_or(0), kMin);
    ZTH_EXPECT_EQ(ts::align_up(kMin, 1000).value_or(0), kMin + 808);
    ZTH_EXPECT_FALSE(ts::align_up(kMax, 1000).has_value());
    ZTH_EXPECT_EQ(ts::align_up(kMax, 1).value_or(0), kMax);
    ZTH_EXPECT_EQ(ts::align_down(kMax, 1000).value_or(0), kMax - 807);
    ZTH_EXPECT_EQ(ts::align_up(kMax - 807, 1000).value_or(0), kMax - 807);
    ZTH_EXPECT_FALSE(ts::align_up(kMax - 806, 1000).has_value());
    ZTH_EXPECT_EQ(ts::align_down(-5, kMax).value_or(0), -kMax);
    ZTH_EXPECT_FALSE(ts::align_down(kMin, kMax).has_value());   // the multiple below kMin does not exist
    ZTH_EXPECT_EQ(ts::align_up(5, kMax).value_or(0), kMax);
}
ZTH_TEST("robust", "bucket_of across the whole int64 range uses unsigned arithmetic") {
    ZTH_EXPECT_EQ(ts::bucket_of(kMax, kMin, 1).value_or(0), std::uint64_t{0xFFFFFFFFFFFFFFFFull});
    ZTH_EXPECT_EQ(ts::bucket_of(kMax, kMin, 1000).value_or(0), std::uint64_t{0xFFFFFFFFFFFFFFFFull} / 1000);
    ZTH_EXPECT_EQ(ts::bucket_of(0, kMin, kMax).value_or(99), std::uint64_t{1});   // 2^63 / (2^63 - 1) = 1
    ZTH_EXPECT_EQ(ts::bucket_of(kMin, kMin, kMax).value_or(99), std::uint64_t{0});
    ZTH_EXPECT_FALSE(ts::bucket_of(kMin, 0, 5).has_value());
}
ZTH_TEST("robust", "resample against a brute-force 128-bit reference: 500 random series and grids, all fills, random gaps") {
    zth::Lcg rng(612);
    for (int round = 0; round < 500; ++round) {
        std::vector<Sample> raw;
        const int n = rng.below(14);
        for (int i = 0; i < n; ++i) raw.push_back({static_cast<Time>(rng.below(400) - 100) * (rng.below(4) == 0 ? 1000000007LL : 1), (rng.below(2001) - 1000) / 8.0});
        if (rng.below(8) == 0) raw.push_back({kMin, 1.5});
        if (rng.below(8) == 0) raw.push_back({kMax, -2.5});
        const std::vector<Sample> s = ts::normalize(raw);
        const Time start = (rng.below(300) - 120) * (rng.below(5) == 0 ? 3000000019LL : 1);
        const Time step = 1 + rng.below(60) * (rng.below(5) == 0 ? 1000003 : 1);
        const std::size_t count = static_cast<std::size_t>(rng.below(30));
        const Time gap = rng.below(3) == 0 ? ts::kNoLimit : rng.below(500);
        for (Fill f : {Fill::None, Fill::Previous, Fill::Linear}) {
            const auto got = ts::resample(s, start, step, count, f, gap);
            ZTH_EXPECT_TRUE(got.has_value());
            ZTH_EXPECT_TRUE(got && same(*got, ref_resample(s, start, step, count, f, gap), 1e-9));
        }
    }
}
ZTH_TEST("robust", "differences larger than int64: interpolating between kMin and kMax, carrying across the whole range") {
    const std::vector<Sample> wide = {{kMin, 0.0}, {kMax, 100.0}};
    const auto mid = ts::resample(wide, -1, 1, 3, Fill::Linear);   // -1, 0, 1: the middle of the range
    ZTH_EXPECT_TRUE(mid.has_value());
    ZTH_EXPECT_NEAR((*mid)[1], 50.0, 1e-9);
    ZTH_EXPECT_NEAR((*mid)[0], 50.0, 1e-9);
    const auto prev = ts::resample(wide, kMax - 2, 1, 3, Fill::Previous);   // 2^64 - 3 after the first sample, but an exact hit at the end
    ZTH_EXPECT_TRUE(prev.has_value());
    ZTH_EXPECT_NEAR((*prev)[0], 0.0, 0);
    ZTH_EXPECT_NEAR((*prev)[2], 100.0, 0);
    ZTH_EXPECT_TRUE(same(grid(wide, kMax - 2, 1, 3, Fill::Previous, 100), Vec{kNan, kNan, 100}));   // the first sample is far further back than 100
    ZTH_EXPECT_TRUE(same(grid(wide, 0, 1, 1, Fill::Linear, kMax - 1), Vec{kNan}));   // the two samples are 2^64 - 1 apart: more than any finite max_gap
}
ZTH_TEST("robust", "the grid may end exactly at INT64_MAX but not pass it; huge steps and counts are refused without wrapping around") {
    const std::vector<Sample> s = {{kMax - 10, 4.0}};
    const auto ok = ts::resample(s, kMax - 10, 5, 3, Fill::Previous);   // kMax - 10, kMax - 5, kMax
    ZTH_EXPECT_TRUE(ok.has_value() && same(*ok, Vec{4, 4, 4}));
    ZTH_EXPECT_FALSE(ts::resample(s, kMax - 10, 5, 4, Fill::Previous).has_value());   // the 4th point would be kMax + 5
    ZTH_EXPECT_FALSE(ts::resample(s, kMax, 1, 2, Fill::None).has_value());
    ZTH_EXPECT_TRUE(ts::resample(s, kMax, 1, 1, Fill::None).has_value());
    ZTH_EXPECT_FALSE(ts::resample(s, kMin, kMax, 4, Fill::None).has_value());   // 3 * kMax passes the end
    ZTH_EXPECT_TRUE(ts::resample(s, kMin, kMax, 3, Fill::None).has_value() == true);   // kMin + 2 * kMax = kMax - 1
    ZTH_EXPECT_FALSE(ts::resample(s, 0, kMax, 3, Fill::None).has_value());
    ZTH_EXPECT_FALSE(ts::resample(s, -5, std::numeric_limits<Time>::max() / 2 + 1, 4, Fill::None).has_value());
    ZTH_EXPECT_FALSE(ts::resample(s, kMin, 1, ts::kMaxPoints + 1, Fill::None).has_value());
}
ZTH_TEST("robust", "negative times: the grid and the buckets work before the epoch") {
    const std::vector<Sample> s = {{-3000, 1}, {-1000, 3}, {1000, 5}};
    ZTH_EXPECT_TRUE(same(grid(s, -4000, 1000, 7, Fill::Linear), Vec{kNan, 1, 2, 3, 4, 5, kNan}));
    const auto b = ts::aggregate(s, -4000, 2000, 3);   // [-4000,-2000) [-2000,0) [0,2000)
    ZTH_EXPECT_TRUE(b.has_value());
    ZTH_EXPECT_TRUE((*b)[0].n == 1 && (*b)[0].sum == 1);
    ZTH_EXPECT_TRUE((*b)[1].n == 1 && (*b)[1].sum == 3);
    ZTH_EXPECT_TRUE((*b)[2].n == 1 && (*b)[2].sum == 5);
}
ZTH_TEST("robust", "aggregate against a brute-force reference, including samples at the extremes and buckets that end at INT64_MAX") {
    zth::Lcg rng(77);
    for (int round = 0; round < 300; ++round) {
        std::vector<Sample> raw;
        for (int i = 0, n = rng.below(40); i < n; ++i) raw.push_back({rng.below(300) - 50, (rng.below(401) - 200) / 4.0});
        const std::vector<Sample> s = ts::normalize(raw);
        const Time start = rng.below(120) - 60;
        const Time width = 1 + rng.below(40);
        const std::size_t count = static_cast<std::size_t>(rng.below(12));
        const auto got = ts::aggregate(s, start, width, count);
        ZTH_EXPECT_TRUE(got.has_value() && got->size() == count);
        if (!got) return;
        for (std::size_t k = 0; k < count; ++k) {
            const Time lo = start + static_cast<Time>(k) * width, hi = lo + width;
            std::size_t n = 0;
            double sum = 0, mn = kNan, mx = kNan, first = kNan, last = kNan;
            for (const Sample& x : s)
                if (x.t >= lo && x.t < hi) {
                    if (n == 0) first = mn = mx = x.v;
                    mn = std::min(mn, x.v);
                    mx = std::max(mx, x.v);
                    last = x.v;
                    sum += x.v;
                    ++n;
                }
            const Bucket& b = (*got)[k];
            ZTH_EXPECT_EQ(b.n, n);
            ZTH_EXPECT_NEAR(b.sum, sum, 1e-9);
            ZTH_EXPECT_TRUE(same(Vec{b.min, b.max, b.first, b.last}, Vec{mn, mx, first, last}));
        }
    }
    const std::vector<Sample> edge = {{kMax, 9.0}, {kMax - 1, 8.0}, {kMin, 7.0}};
    const std::vector<Sample> sorted = ts::normalize(edge);
    const auto top = ts::aggregate(sorted, kMax - 3, 2, 2);   // [kMax-3, kMax-1) and [kMax-1, kMax+1): the second ends beyond INT64_MAX
    ZTH_EXPECT_FALSE(top.has_value());
    const auto top_ok = ts::aggregate(sorted, kMax - 4, 2, 2);   // ends exactly at kMax
    ZTH_EXPECT_TRUE(top_ok.has_value());
    ZTH_EXPECT_FALSE(ts::aggregate(sorted, kMin, kMax, 3).has_value());
    const auto low = ts::aggregate(sorted, kMin, 1, 1);
    ZTH_EXPECT_TRUE(low.has_value() && (*low)[0].n == 1 && (*low)[0].sum == 7);
}

// ================================================ Fast ========================================================
ZTH_TEST("fast", "resample allocates only its result; aggregate only its buckets; normalize at most a sort buffer") {
    std::vector<Sample> s;
    for (int i = 0; i < 5000; ++i) s.push_back({i * 10, static_cast<double>(i)});
    zth::mem::Scope a;
    const auto r = ts::resample(s, 0, 7, 8000, Fill::Linear, 25);
    ZTH_EXPECT_ALLOCS_AT_MOST(a, 1);
    zth::mem::Scope b;
    const auto g = ts::aggregate(s, 0, 100, 600);
    ZTH_EXPECT_ALLOCS_AT_MOST(b, 1);
    std::vector<Sample> shuffled = s;
    for (std::size_t i = shuffled.size(); i > 1; --i) std::swap(shuffled[i - 1], shuffled[(i * 2654435761u) % i]);
    zth::mem::Scope c;
    const auto n = ts::normalize(std::move(shuffled));
    ZTH_EXPECT_ALLOCS_AT_MOST(c, 2);
    ZTH_EXPECT_TRUE(r.has_value() && g.has_value() && n.size() == s.size());
}
ZTH_TEST_TIMEOUT("fast", "a million samples onto a two-million-point grid in one pass (no scan per grid point)", 20) {
    std::vector<Sample> s;
    s.reserve(1000000);
    for (int i = 0; i < 1000000; ++i) s.push_back({static_cast<Time>(i) * 13, std::sin(i * 0.001)});
    const auto lin = ts::resample(s, 0, 6, 2000000, Fill::Linear);
    const auto prev = ts::resample(s, 0, 6, 2000000, Fill::Previous, 20);
    ZTH_EXPECT_TRUE(lin.has_value() && prev.has_value());
    ZTH_EXPECT_TRUE(std::isfinite((*lin)[1000001]) && std::isfinite((*prev)[1000001]));
    const auto agg = ts::aggregate(s, 0, 13000, 1000);
    ZTH_EXPECT_TRUE(agg.has_value() && (*agg)[500].n == 1000);
}
ZTH_TEST_TIMEOUT("fast", "normalising a million shuffled samples", 20) {
    std::vector<Sample> s;
    for (int i = 0; i < 1000000; ++i) s.push_back({static_cast<Time>((static_cast<unsigned long long>(i) * 2654435761ull) % 1000003ull), static_cast<double>(i)});
    const auto n = ts::normalize(std::move(s));
    ZTH_EXPECT_TRUE(n.size() <= 1000003 && n.size() > 600000);
    for (std::size_t i = 1; i < n.size(); ++i)
        if (!(n[i - 1].t < n[i].t)) {
            zth::fail("normalize returned a series that is not strictly increasing");
            return;
        }
}

// ================================================ Clean =======================================================
ZTH_TEST("clean", "no raw memory, no exceptions, no goto") {
    ZTH_EXPECT_NO_RAW_MEMORY("timeseries.hpp");
    const std::string code = ::zth::strip_comments_and_literals(::zth::read_file("timeseries.hpp"));
    for (const char* word : {"throw", "try", "catch", "goto"}) ZTH_EXPECT_TRUE(::zth::find_word(code, word).empty());
}
ZTH_TEST("clean", "time arithmetic is integer arithmetic: no floor/fmod/round/pow on timestamps, no containers that search per grid point") {
    const std::string code = ::zth::strip_comments_and_literals(::zth::read_file("timeseries.hpp"));
    for (const char* word : {"floor", "ceil", "fmod", "round", "pow", "lround", "map", "set", "find", "find_if", "lower_bound", "upper_bound", "binary_search"}) ZTH_EXPECT_TRUE(::zth::find_word(code, word).empty());
    ZTH_EXPECT_FALSE(::zth::find_word(code, "uint64_t").empty());
}
