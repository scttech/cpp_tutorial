// MUTANT: a rate of 1 or more is used as it is
#pragma once
// A bit array in 64-bit words, k bit positions per item from double hashing, and a running count of set bits.
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
    BloomFilter(std::size_t bits, std::size_t hashes) : bits_(bits == 0 ? 1 : bits), hashes_(hashes == 0 ? 1 : hashes), words_((bits_ + 63) / 64, 0) {}

    static std::size_t optimal_bits(std::size_t expected_items, double false_positive_rate) {
        const double n = expected_items == 0 ? 1.0 : static_cast<double>(expected_items);
        const double p = false_positive_rate > 0.0 ? false_positive_rate : 0.01;
        const double ln2 = std::log(2.0);
        return static_cast<std::size_t>(std::ceil(-n * std::log(p) / (ln2 * ln2)));
    }

    static std::size_t optimal_hashes(std::size_t bits, std::size_t expected_items) {
        const double n = expected_items == 0 ? 1.0 : static_cast<double>(expected_items);
        const double k = static_cast<double>(bits) / n * std::log(2.0);
        return std::max<std::size_t>(1, static_cast<std::size_t>(std::lround(k)));
    }

    static BloomFilter for_capacity(std::size_t expected_items, double false_positive_rate) {
        const std::size_t bits = optimal_bits(expected_items, false_positive_rate);
        return BloomFilter(bits, optimal_hashes(bits, expected_items));
    }

    void add(std::string_view item) {
        const HashPair h = hash_pair(item);
        for (std::size_t i = 0; i < hashes_; ++i) set_bit(index(h, i));
        ++items_added_;
    }

    bool might_contain(std::string_view item) const {
        const HashPair h = hash_pair(item);
        for (std::size_t i = 0; i < hashes_; ++i) {
            if (!get_bit(index(h, i))) return false;
        }
        return true;
    }

    std::size_t bits() const { return bits_; }
    std::size_t hashes() const { return hashes_; }
    std::size_t items_added() const { return items_added_; }
    std::size_t bits_set() const { return bits_set_; }
    double fill_ratio() const { return static_cast<double>(bits_set_) / static_cast<double>(bits_); }
    double estimated_fpr() const { return std::pow(fill_ratio(), static_cast<double>(hashes_)); }

    double estimated_count() const {
        const double ratio = fill_ratio();
        return -(static_cast<double>(bits_) / static_cast<double>(hashes_)) * std::log(1.0 - ratio);
    }

    bool merge(const BloomFilter& other) {
        if (bits_ != other.bits_ || hashes_ != other.hashes_) return false;
        std::size_t set = 0;
        for (std::size_t i = 0; i < words_.size(); ++i) {
            words_[i] |= other.words_[i];
            set += static_cast<std::size_t>(std::popcount(words_[i]));
        }
        bits_set_ = set;
        items_added_ += other.items_added_;
        return true;
    }

    void clear() {
        std::fill(words_.begin(), words_.end(), 0);
        bits_set_ = 0;
        items_added_ = 0;
    }

    std::vector<std::uint8_t> to_bytes() const {
        const std::size_t body = (bits_ + 7) / 8;
        std::vector<std::uint8_t> out;
        out.reserve(12 + body);
        for (std::size_t i = 0; i < 8; ++i) out.push_back(static_cast<std::uint8_t>((static_cast<std::uint64_t>(bits_) >> (8 * i)) & 0xFF));
        for (std::size_t i = 0; i < 4; ++i) out.push_back(static_cast<std::uint8_t>((static_cast<std::uint64_t>(hashes_) >> (8 * i)) & 0xFF));
        for (std::size_t b = 0; b < body; ++b) out.push_back(static_cast<std::uint8_t>((words_[b / 8] >> (8 * (b % 8))) & 0xFF));
        return out;
    }

    static std::optional<BloomFilter> from_bytes(std::span<const std::uint8_t> data) {
        if (data.size() < 12) return std::nullopt;
        std::uint64_t bits = 0, hashes = 0;
        for (std::size_t i = 0; i < 8; ++i) bits |= static_cast<std::uint64_t>(data[i]) << (8 * i);
        for (std::size_t i = 0; i < 4; ++i) hashes |= static_cast<std::uint64_t>(data[8 + i]) << (8 * i);
        if (bits == 0 || hashes == 0) return std::nullopt;
        const std::uint64_t body = data.size() - 12;
        // Check against the data we really have BEFORE allocating anything for `bits` bits.
        if (bits > body * 8 || (bits + 7) / 8 != body) return std::nullopt;
        if (bits % 8 != 0 && (data.back() >> (bits % 8)) != 0) return std::nullopt;  // the unused bits of the last byte must be 0
        BloomFilter f(static_cast<std::size_t>(bits), static_cast<std::size_t>(hashes));
        for (std::size_t b = 0; b < body; ++b) f.words_[b / 8] |= static_cast<std::uint64_t>(data[12 + b]) << (8 * (b % 8));
        std::size_t set = 0;
        for (std::uint64_t w : f.words_) set += static_cast<std::size_t>(std::popcount(w));
        f.bits_set_ = set;
        return f;
    }

private:
    std::size_t index(const HashPair& h, std::size_t i) const { return static_cast<std::size_t>((h.h1 + static_cast<std::uint64_t>(i) * h.h2) % bits_); }

    bool get_bit(std::size_t idx) const { return (words_[idx / 64] >> (idx % 64)) & 1ULL; }

    void set_bit(std::size_t idx) {
        const std::uint64_t mask = 1ULL << (idx % 64);
        std::uint64_t& word = words_[idx / 64];
        if ((word & mask) == 0) {
            word |= mask;
            ++bits_set_;
        }
    }

    std::size_t bits_;
    std::size_t hashes_;
    std::vector<std::uint64_t> words_;
    std::size_t bits_set_ = 0;
    std::size_t items_added_ = 0;
};

}  // namespace bloom
