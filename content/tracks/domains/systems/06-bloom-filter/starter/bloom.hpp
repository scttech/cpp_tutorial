#pragma once
// YOUR BLOOM FILTER: a set that answers "definitely not present" or "probably present", in a tiny fixed amount of memory.
// It never forgets a member (NO false negatives) but can claim a stranger is present (a false positive) with a small, tunable probability.
//
//   BloomFilter(bits, hashes)                     bits and hashes are at least 1 (0 is treated as 1)
//   static optimal_bits(n, p)                     m = ceil( -n * ln(p) / (ln 2)^2 )      n == 0 counts as 1; p not in (0, 1) counts as 0.01
//   static optimal_hashes(bits, n)                k = round( bits / n * ln 2 ), at least 1   (n == 0 counts as 1)
//   static for_capacity(n, p)                     a filter with those two numbers
//   add(item)                                     sets bits  (h1 + i * h2) mod bits  for i = 0 .. hashes-1   (hash_pair in types.hpp, uint64 wrap-around)
//   might_contain(item)                           true iff all those bits are set
//   bits(), hashes(), items_added() (calls of add, duplicates included), bits_set() (O(1)!), fill_ratio() = bits_set / bits
//   estimated_fpr()                               fill_ratio ^ hashes
//   estimated_count()                             - (bits / hashes) * ln(1 - bits_set / bits)     (+infinity when every bit is set)
//   merge(other)                                  union of two filters of the SAME shape (bits and hashes): false and unchanged otherwise; items_added adds up
//   clear()
//   to_bytes()                                    8 bytes bits (little endian), 4 bytes hashes (little endian), then ceil(bits/8) bytes: bit i is bit (i % 8) of byte (i / 8)
//   from_bytes(span)                              the inverse; std::nullopt for ANY malformed input (see the lesson), without allocating anything first
#include <algorithm>
#include <bit>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <optional>
#include <span>
#include <string_view>
#include <vector>

#include "types.hpp"

namespace bloom {

class BloomFilter {
public:
    BloomFilter(std::size_t bits, std::size_t hashes) : bits_(bits == 0 ? 1 : bits), hashes_(hashes == 0 ? 1 : hashes) {}

    static std::size_t optimal_bits(std::size_t expected_items, double false_positive_rate) {
        (void)expected_items;
        (void)false_positive_rate;
        return 0;  // TODO
    }
    static std::size_t optimal_hashes(std::size_t bits, std::size_t expected_items) {
        (void)bits;
        (void)expected_items;
        return 0;  // TODO
    }
    static BloomFilter for_capacity(std::size_t expected_items, double false_positive_rate) {
        return BloomFilter(optimal_bits(expected_items, false_positive_rate), optimal_hashes(optimal_bits(expected_items, false_positive_rate), expected_items));
    }

    void add(std::string_view item) { (void)item; }  // TODO
    bool might_contain(std::string_view item) const {
        (void)item;
        return false;  // TODO
    }

    std::size_t bits() const { return bits_; }
    std::size_t hashes() const { return hashes_; }
    std::size_t items_added() const { return 0; }  // TODO
    std::size_t bits_set() const { return 0; }     // TODO
    double fill_ratio() const { return 0.0; }      // TODO
    double estimated_fpr() const { return 0.0; }   // TODO
    double estimated_count() const { return 0.0; } // TODO

    bool merge(const BloomFilter& other) {
        (void)other;
        return false;  // TODO
    }
    void clear() {}  // TODO

    std::vector<std::uint8_t> to_bytes() const { return {}; }  // TODO
    static std::optional<BloomFilter> from_bytes(std::span<const std::uint8_t> data) {
        (void)data;
        return std::nullopt;  // TODO
    }

private:
    std::size_t bits_;
    std::size_t hashes_;
    // TODO: the bit array (std::vector<std::uint64_t> words), a running count of set bits, the number of add() calls
};

}  // namespace bloom
