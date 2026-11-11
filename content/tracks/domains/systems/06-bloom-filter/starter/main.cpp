// A Bloom filter in front of a slow database: "definitely not here" answers without a lookup. Read-only. Press Run.
#include <cstdio>
#include <string>

#include "bloom.hpp"

int main() {
    bloom::BloomFilter filter = bloom::BloomFilter::for_capacity(10000, 0.01);   // 10000 items, 1% false positives
    std::printf("%zu bits (%zu bytes), %zu hash functions\n", filter.bits(), filter.bits() / 8, filter.hashes());
    for (int i = 0; i < 10000; ++i) filter.add("user:" + std::to_string(i));

    int missing_reported_present = 0;
    for (int i = 0; i < 10000; ++i) missing_reported_present += filter.might_contain("user:" + std::to_string(i)) ? 0 : 1;
    std::printf("members the filter forgot (must be 0): %d\n", missing_reported_present);

    int false_positives = 0;
    for (int i = 10000; i < 60000; ++i) false_positives += filter.might_contain("user:" + std::to_string(i)) ? 1 : 0;
    std::printf("false positives among 50000 strangers: %d (%.2f%%)\n", false_positives, 100.0 * false_positives / 50000);
    std::printf("bits set: %zu of %zu (%.1f%%), estimated items: %.0f, estimated false-positive rate: %.4f\n", filter.bits_set(), filter.bits(), 100.0 * filter.fill_ratio(),
                filter.estimated_count(), filter.estimated_fpr());
    const auto bytes = filter.to_bytes();
    const auto copy = bloom::BloomFilter::from_bytes(bytes);
    std::printf("serialized to %zu bytes; reloaded: %s\n", bytes.size(), copy && copy->might_contain("user:42") ? "yes" : "no");
}
