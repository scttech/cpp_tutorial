// Irregular sensor readings -> a regular grid, and downsampling into buckets. Read-only. Press Run.
#include <cmath>
#include <cstdio>
#include <vector>

#include "timeseries.hpp"

static void show(const char* name, const std::vector<double>& v) {
    std::printf("%-26s", name);
    for (double x : v) {
        if (std::isnan(x)) std::printf("     --");
        else std::printf(" %6.2f", x);
    }
    std::printf("\n");
}

int main() {
    // readings arrive out of order, one is repeated, one is garbage (times in milliseconds)
    const std::vector<ts::Sample> raw = {{4000, 0}, {0, 10}, {1000, 20}, {5000, 50}, {1000, 21}, {2500, std::nan("")}};
    const std::vector<ts::Sample> s = ts::normalize(raw);
    std::printf("normalised: %zu samples\n", s.size());
    std::printf("%-26s", "grid time (s)");
    for (int k = 0; k < 7; ++k) std::printf(" %6d", k);
    std::printf("\n");
    if (const auto r = ts::resample(s, 0, 1000, 7, ts::Fill::None)) show("exact matches only", *r);
    if (const auto r = ts::resample(s, 0, 1000, 7, ts::Fill::Previous)) show("carry the last value", *r);
    if (const auto r = ts::resample(s, 0, 1000, 7, ts::Fill::Previous, 1000)) show("carry, at most 1 s", *r);
    if (const auto r = ts::resample(s, 0, 1000, 7, ts::Fill::Linear)) show("linear", *r);
    if (const auto r = ts::resample(s, 0, 1000, 7, ts::Fill::Linear, 2000)) show("linear, gaps <= 2 s", *r);

    if (const auto b = ts::aggregate(s, 0, 2500, 3)) {
        for (std::size_t i = 0; i < b->size(); ++i) {
            const ts::Bucket& k = (*b)[i];
            std::printf("bucket %zu [%lld, %lld): n=%zu mean=%.2f min=%.2f max=%.2f\n", i, static_cast<long long>(i * 2500), static_cast<long long>((i + 1) * 2500), k.n, k.mean(), k.min, k.max);
        }
    }
    std::printf("align_down(-1, 1000) = %lld   (the start of the second before time zero)\n", static_cast<long long>(ts::align_down(-1, 1000).value_or(-1)));
}
