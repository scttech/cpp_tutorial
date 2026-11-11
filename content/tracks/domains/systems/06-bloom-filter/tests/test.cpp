#define ZTH_TRACK_ALLOCATIONS
#include <cmath>
#include <set>
#include <string>
#include <vector>

#include "bloom.hpp"
#include "types.hpp"
#include "zth_test.hpp"

using namespace bloom;

namespace {

// An independent model: positions computed from hash_pair, bits in a vector<bool>.
struct Model {
    std::size_t bits, hashes;
    std::vector<bool> set;
    Model(std::size_t b, std::size_t k) : bits(b == 0 ? 1 : b), hashes(k == 0 ? 1 : k), set(bits, false) {}
    void add(const std::string& item) {
        const HashPair h = hash_pair(item);
        for (std::size_t i = 0; i < hashes; ++i) set[static_cast<std::size_t>((h.h1 + static_cast<std::uint64_t>(i) * h.h2) % bits)] = true;
    }
    std::size_t count() const {
        std::size_t c = 0;
        for (bool b : set) c += b;
        return c;
    }
    std::vector<std::uint8_t> bytes() const {
        std::vector<std::uint8_t> out;
        for (std::size_t i = 0; i < 8; ++i) out.push_back(static_cast<std::uint8_t>((static_cast<std::uint64_t>(bits) >> (8 * i)) & 0xFF));
        for (std::size_t i = 0; i < 4; ++i) out.push_back(static_cast<std::uint8_t>((static_cast<std::uint64_t>(hashes) >> (8 * i)) & 0xFF));
        for (std::size_t b = 0; b < (bits + 7) / 8; ++b) {
            std::uint8_t v = 0;
            for (std::size_t j = 0; j < 8; ++j)
                if (b * 8 + j < bits && set[b * 8 + j]) v = static_cast<std::uint8_t>(v | (1u << j));
            out.push_back(v);
        }
        return out;
    }
};

std::string member(int i) { return "member-" + std::to_string(i); }
std::string stranger(int i) { return "stranger-" + std::to_string(i); }

}  // namespace

// ================================================ Works =======================================================
ZTH_TEST("works", "a new filter is empty") {
    BloomFilter f(1000, 3);
    ZTH_EXPECT_EQ(f.bits(), std::size_t{1000});
    ZTH_EXPECT_EQ(f.hashes(), std::size_t{3});
    ZTH_EXPECT_EQ(f.bits_set(), std::size_t{0});
    ZTH_EXPECT_EQ(f.items_added(), std::size_t{0});
    ZTH_EXPECT_EQ(f.fill_ratio(), 0.0);
    ZTH_EXPECT_EQ(f.estimated_fpr(), 0.0);
    ZTH_EXPECT_EQ(f.estimated_count(), 0.0);
    for (int i = 0; i < 100; ++i) ZTH_EXPECT_FALSE(f.might_contain(stranger(i)));
}
ZTH_TEST("works", "zero bits or zero hashes are treated as one") {
    BloomFilter f(0, 0);
    ZTH_EXPECT_EQ(f.bits(), std::size_t{1});
    ZTH_EXPECT_EQ(f.hashes(), std::size_t{1});
    ZTH_EXPECT_FALSE(f.might_contain("x"));
    f.add("x");
    ZTH_EXPECT_TRUE(f.might_contain("x"));
    ZTH_EXPECT_TRUE(f.might_contain("anything at all"));   // one bit: once set, everything "might be" present
    ZTH_EXPECT_EQ(f.bits_set(), std::size_t{1});
}
ZTH_TEST("works", "an added item is found; items that were never added are (in a roomy filter) not") {
    BloomFilter f(20000, 5);
    for (int i = 0; i < 10; ++i) f.add(member(i));
    for (int i = 0; i < 10; ++i) ZTH_EXPECT_TRUE(f.might_contain(member(i)));
    for (int i = 0; i < 100; ++i) ZTH_EXPECT_FALSE(f.might_contain(stranger(i)));
    ZTH_EXPECT_EQ(f.items_added(), std::size_t{10});
}
ZTH_TEST("works", "an item sets exactly the bits (h1 + i * h2) mod bits, checked through the serialized form") {
    for (std::size_t bits : {std::size_t{100}, std::size_t{64}, std::size_t{13}}) {
        BloomFilter f(bits, 3);
        Model m(bits, 3);
        for (const char* item : {"abc", "", "another item", "x"}) {
            f.add(item);
            m.add(item);
            if (f.to_bytes() != m.bytes()) {
                zth::fail("after adding `" + std::string(item) + "` (" + std::to_string(bits) + " bits) the set bits differ from the model");
                return;
            }
        }
        ZTH_EXPECT_EQ(f.bits_set(), m.count());
    }
}
ZTH_TEST("works", "bits_set counts distinct bits: adding the same item again changes nothing, items_added counts calls") {
    BloomFilter f(5000, 4);
    f.add("same");
    const std::size_t once = f.bits_set();
    ZTH_EXPECT_TRUE(once >= 1 && once <= 4);
    f.add("same");
    f.add("same");
    ZTH_EXPECT_EQ(f.bits_set(), once);
    ZTH_EXPECT_EQ(f.items_added(), std::size_t{3});
}
ZTH_TEST("works", "empty items and items with zero bytes are ordinary items") {
    BloomFilter f(5000, 4);
    f.add("");
    f.add(std::string("a\0b", 3));
    ZTH_EXPECT_TRUE(f.might_contain(""));
    ZTH_EXPECT_TRUE(f.might_contain(std::string("a\0b", 3)));
    ZTH_EXPECT_FALSE(f.might_contain("a"));
    ZTH_EXPECT_FALSE(f.might_contain(std::string("a\0c", 3)));
}
ZTH_TEST("works", "clear empties the filter") {
    BloomFilter f(1000, 3);
    for (int i = 0; i < 50; ++i) f.add(member(i));
    f.clear();
    ZTH_EXPECT_EQ(f.bits_set(), std::size_t{0});
    ZTH_EXPECT_EQ(f.items_added(), std::size_t{0});
    ZTH_EXPECT_FALSE(f.might_contain(member(3)));
    ZTH_EXPECT_EQ(f.bits(), std::size_t{1000});
    f.add(member(3));
    ZTH_EXPECT_TRUE(f.might_contain(member(3)));
    ZTH_EXPECT_TRUE(f.bits_set() >= 1);
}
ZTH_TEST("works", "optimal_bits and optimal_hashes follow the formulas, rounding up and to nearest") {
    ZTH_EXPECT_EQ(BloomFilter::optimal_bits(1000, 0.01), std::size_t{9586});      // 9585.06
    ZTH_EXPECT_EQ(BloomFilter::optimal_hashes(9586, 1000), std::size_t{7});       // 6.64
    ZTH_EXPECT_EQ(BloomFilter::optimal_bits(10000, 0.001), std::size_t{143776});  // 143775.88
    ZTH_EXPECT_EQ(BloomFilter::optimal_hashes(143776, 10000), std::size_t{10});   // 9.97
    ZTH_EXPECT_EQ(BloomFilter::optimal_bits(100, 0.05), std::size_t{624});        // 623.52
    ZTH_EXPECT_EQ(BloomFilter::optimal_hashes(624, 100), std::size_t{4});         // 4.33
    ZTH_EXPECT_EQ(BloomFilter::optimal_bits(500, 0.1), std::size_t{2397});        // 2396.26
    ZTH_EXPECT_EQ(BloomFilter::optimal_hashes(2397, 500), std::size_t{3});        // 3.32
    ZTH_EXPECT_EQ(BloomFilter::optimal_bits(1000000, 0.01), std::size_t{9585059});
}
ZTH_TEST("works", "optimal_*: zero items count as one, bad rates count as 1%, at least one hash") {
    ZTH_EXPECT_EQ(BloomFilter::optimal_bits(0, 0.01), std::size_t{10});
    ZTH_EXPECT_EQ(BloomFilter::optimal_bits(1, 0.01), std::size_t{10});
    ZTH_EXPECT_EQ(BloomFilter::optimal_bits(1000, 0.0), BloomFilter::optimal_bits(1000, 0.01));
    ZTH_EXPECT_EQ(BloomFilter::optimal_bits(1000, 1.0), BloomFilter::optimal_bits(1000, 0.01));
    ZTH_EXPECT_EQ(BloomFilter::optimal_bits(1000, -3.0), BloomFilter::optimal_bits(1000, 0.01));
    ZTH_EXPECT_EQ(BloomFilter::optimal_bits(1000, 1.5), BloomFilter::optimal_bits(1000, 0.01));
    ZTH_EXPECT_EQ(BloomFilter::optimal_hashes(10, 100), std::size_t{1});        // 0.07 rounds to 0: but never fewer than 1
    ZTH_EXPECT_EQ(BloomFilter::optimal_hashes(1000, 0), BloomFilter::optimal_hashes(1000, 1));
}
ZTH_TEST("works", "for_capacity builds a filter of that shape") {
    BloomFilter f = BloomFilter::for_capacity(1000, 0.01);
    ZTH_EXPECT_EQ(f.bits(), std::size_t{9586});
    ZTH_EXPECT_EQ(f.hashes(), std::size_t{7});
    BloomFilter g = BloomFilter::for_capacity(0, 0.5);
    ZTH_EXPECT_TRUE(g.bits() >= 1 && g.hashes() >= 1);
}
ZTH_TEST("works", "fill ratio, estimated false-positive rate and estimated count follow their formulas") {
    BloomFilter f(1000, 4);
    for (int i = 0; i < 100; ++i) f.add(member(i));
    const double x = static_cast<double>(f.bits_set());
    ZTH_EXPECT_NEAR(f.fill_ratio(), x / 1000.0, 1e-12);
    ZTH_EXPECT_NEAR(f.estimated_fpr(), std::pow(x / 1000.0, 4.0), 1e-12);
    ZTH_EXPECT_NEAR(f.estimated_count(), -(1000.0 / 4.0) * std::log(1.0 - x / 1000.0), 1e-9);
    ZTH_EXPECT_TRUE(f.estimated_count() > 90.0 && f.estimated_count() < 110.0);
}
ZTH_TEST("works", "a filter with every bit set estimates an infinite count") {
    BloomFilter f(8, 2);
    for (int i = 0; i < 2000; ++i) f.add(member(i));
    ZTH_EXPECT_EQ(f.bits_set(), std::size_t{8});
    ZTH_EXPECT_EQ(f.fill_ratio(), 1.0);
    ZTH_EXPECT_TRUE(std::isinf(f.estimated_count()));
    ZTH_EXPECT_TRUE(f.estimated_count() > 0);
    ZTH_EXPECT_EQ(f.estimated_fpr(), 1.0);
}
ZTH_TEST("works", "merge is a union: it knows everything either filter knew") {
    BloomFilter a(20000, 5), b(20000, 5), both(20000, 5);
    for (int i = 0; i < 300; ++i) {
        (i % 2 == 0 ? a : b).add(member(i));
        both.add(member(i));
    }
    ZTH_EXPECT_TRUE(a.merge(b));
    ZTH_EXPECT_TRUE(a.to_bytes() == both.to_bytes());
    ZTH_EXPECT_EQ(a.bits_set(), both.bits_set());
    ZTH_EXPECT_EQ(a.items_added(), std::size_t{300});
    for (int i = 0; i < 300; ++i) ZTH_EXPECT_TRUE(a.might_contain(member(i)));
    ZTH_EXPECT_EQ(b.items_added(), std::size_t{150});   // the other filter is untouched
}
ZTH_TEST("works", "merge refuses filters of another shape and changes nothing") {
    BloomFilter a(1000, 3), b(1001, 3), c(1000, 4);
    a.add("x");
    b.add("y");
    c.add("z");
    const auto before = a.to_bytes();
    ZTH_EXPECT_FALSE(a.merge(b));
    ZTH_EXPECT_FALSE(a.merge(c));
    ZTH_EXPECT_TRUE(a.to_bytes() == before);
    ZTH_EXPECT_EQ(a.items_added(), std::size_t{1});
}
ZTH_TEST("works", "to_bytes layout: 8 bytes of bits and 4 of hashes (little endian), then the bits, lowest bit first") {
    BloomFilter f(100, 3);
    const auto empty = f.to_bytes();
    ZTH_EXPECT_EQ(empty.size(), std::size_t{12 + 13});
    ZTH_EXPECT_EQ(static_cast<int>(empty[0]), 100);
    for (std::size_t i = 1; i < 8; ++i) ZTH_EXPECT_EQ(static_cast<int>(empty[i]), 0);
    ZTH_EXPECT_EQ(static_cast<int>(empty[8]), 3);
    for (std::size_t i = 9; i < empty.size(); ++i) ZTH_EXPECT_EQ(static_cast<int>(empty[i]), 0);
    BloomFilter big(70000, 258);   // 70000 = 0x11170, 258 = 0x102
    const auto b = big.to_bytes();
    ZTH_EXPECT_EQ(static_cast<int>(b[0]), 0x70);
    ZTH_EXPECT_EQ(static_cast<int>(b[1]), 0x11);
    ZTH_EXPECT_EQ(static_cast<int>(b[2]), 0x01);
    ZTH_EXPECT_EQ(static_cast<int>(b[8]), 0x02);
    ZTH_EXPECT_EQ(static_cast<int>(b[9]), 0x01);
    ZTH_EXPECT_EQ(b.size(), std::size_t{12 + 8750});
}
ZTH_TEST("works", "bit order: bit 0 is the lowest bit of the first byte, bit 9 is bit 1 of the second") {
    // find items that hit single known bits in a 1-hash filter of 16 bits
    BloomFilter f(16, 1);
    Model m(16, 1);
    for (int i = 0; i < 40; ++i) {
        f.add(member(i));
        m.add(member(i));
    }
    const auto bytes = f.to_bytes();
    ZTH_EXPECT_TRUE(bytes == m.bytes());
    ZTH_EXPECT_TRUE(bytes[12] != 0 && bytes[13] != 0);
}
ZTH_TEST("works", "a serialized filter loads back identical: same bits, same answers") {
    BloomFilter f(5000, 6);
    for (int i = 0; i < 300; ++i) f.add(member(i));
    const auto bytes = f.to_bytes();
    const auto g = BloomFilter::from_bytes(bytes);
    ZTH_EXPECT_TRUE(g.has_value());
    if (!g) return;
    ZTH_EXPECT_EQ(g->bits(), std::size_t{5000});
    ZTH_EXPECT_EQ(g->hashes(), std::size_t{6});
    ZTH_EXPECT_EQ(g->bits_set(), f.bits_set());
    ZTH_EXPECT_TRUE(g->to_bytes() == bytes);
    for (int i = 0; i < 300; ++i) ZTH_EXPECT_TRUE(g->might_contain(member(i)));
    for (int i = 0; i < 300; ++i) ZTH_EXPECT_EQ(g->might_contain(stranger(i)), f.might_contain(stranger(i)));
    ZTH_EXPECT_EQ(g->items_added(), std::size_t{0});   // the count of add() calls is not stored
}
ZTH_TEST("works", "from_bytes rejects anything malformed") {
    BloomFilter f(100, 3);
    f.add("x");
    const std::vector<std::uint8_t> good = f.to_bytes();
    ZTH_EXPECT_TRUE(BloomFilter::from_bytes(good).has_value());
    ZTH_EXPECT_FALSE(BloomFilter::from_bytes(std::vector<std::uint8_t>{}).has_value());
    ZTH_EXPECT_FALSE(BloomFilter::from_bytes(std::vector<std::uint8_t>(good.begin(), good.begin() + 11)).has_value());   // header cut
    {
        auto v = good;
        v.pop_back();   // one byte short
        ZTH_EXPECT_FALSE(BloomFilter::from_bytes(v).has_value());
    }
    {
        auto v = good;
        v.push_back(0);   // one byte too many
        ZTH_EXPECT_FALSE(BloomFilter::from_bytes(v).has_value());
    }
    {
        auto v = good;
        for (std::size_t i = 0; i < 8; ++i) v[i] = 0;   // zero bits
        ZTH_EXPECT_FALSE(BloomFilter::from_bytes(v).has_value());
    }
    {
        auto v = good;
        for (std::size_t i = 8; i < 12; ++i) v[i] = 0;   // zero hashes
        ZTH_EXPECT_FALSE(BloomFilter::from_bytes(v).has_value());
    }
    {
        auto v = good;
        v.back() = static_cast<std::uint8_t>(v.back() | 0x80);   // 100 bits: the top 4 bits of the last byte must be zero
        ZTH_EXPECT_FALSE(BloomFilter::from_bytes(v).has_value());
    }
    {
        auto v = good;
        v.back() = static_cast<std::uint8_t>(v.back() | 0x08);   // bit 99 is a real bit: fine
        ZTH_EXPECT_TRUE(BloomFilter::from_bytes(v).has_value());
    }
}

// ================================================ Robust ======================================================
ZTH_TEST("robust", "no false negatives: 50000 members, every one is found") {
    BloomFilter f = BloomFilter::for_capacity(50000, 0.01);
    for (int i = 0; i < 50000; ++i) f.add(member(i));
    for (int i = 0; i < 50000; ++i) {
        if (!f.might_contain(member(i))) {
            zth::fail(member(i) + " was added and then forgotten");
            return;
        }
    }
}
ZTH_TEST("robust", "the false-positive rate is what was asked for: about 1% for a filter built for 10000 items and filled with 10000") {
    BloomFilter f = BloomFilter::for_capacity(10000, 0.01);
    for (int i = 0; i < 10000; ++i) f.add(member(i));
    int fp = 0;
    const int kTrials = 200000;
    for (int i = 0; i < kTrials; ++i) fp += f.might_contain(stranger(i));
    const double rate = static_cast<double>(fp) / kTrials;
    ZTH_EXPECT_TRUE(rate > 0.004);
    ZTH_EXPECT_TRUE(rate < 0.02);
    ZTH_EXPECT_TRUE(f.estimated_fpr() > 0.004 && f.estimated_fpr() < 0.02);
    ZTH_EXPECT_NEAR(f.estimated_fpr(), rate, 0.006);
    ZTH_EXPECT_TRUE(f.fill_ratio() > 0.4 && f.fill_ratio() < 0.6);   // an optimal filter is half full
}
ZTH_TEST("robust", "a tighter filter has fewer false positives: 0.1% and 5%") {
    for (double want : {0.001, 0.05}) {
        BloomFilter f = BloomFilter::for_capacity(20000, want);
        for (int i = 0; i < 20000; ++i) f.add(member(i));
        int fp = 0;
        for (int i = 0; i < 200000; ++i) fp += f.might_contain(stranger(i));
        const double rate = static_cast<double>(fp) / 200000.0;
        if (rate > want * 2.5 + 0.0003 || rate < want * 0.3) {
            zth::fail("asked for " + std::to_string(want) + ", measured " + std::to_string(rate));
            return;
        }
    }
}
ZTH_TEST("robust", "estimated_count recovers the number of distinct items (within 3%, 8% for a hundred), duplicates do not inflate it") {
    for (int n : {100, 2000, 10000}) {
        BloomFilter f = BloomFilter::for_capacity(static_cast<std::size_t>(n) * 2, 0.01);
        for (int round = 0; round < 3; ++round)
            for (int i = 0; i < n; ++i) f.add(member(i));
        ZTH_EXPECT_EQ(f.items_added(), static_cast<std::size_t>(n) * 3);
        const double est = f.estimated_count();
        const double tolerance = n < 1000 ? 0.08 : 0.03;   // a small set has more sampling noise
        if (est < n * (1.0 - tolerance) || est > n * (1.0 + tolerance)) {
            zth::fail("n = " + std::to_string(n) + ", estimated " + std::to_string(est));
            return;
        }
    }
}
ZTH_TEST("robust", "word boundaries: sizes 1, 63, 64, 65, 127, 128, 129 and 1000 agree with the model, bit for bit") {
    for (std::size_t bits : {std::size_t{1}, std::size_t{7}, std::size_t{8}, std::size_t{9}, std::size_t{63}, std::size_t{64}, std::size_t{65}, std::size_t{127}, std::size_t{128},
                             std::size_t{129}, std::size_t{1000}}) {
        for (std::size_t k : {std::size_t{1}, std::size_t{2}, std::size_t{5}}) {
            BloomFilter f(bits, k);
            Model m(bits, k);
            for (int i = 0; i < 60; ++i) {
                f.add(member(i));
                m.add(member(i));
            }
            if (f.to_bytes() != m.bytes() || f.bits_set() != m.count()) {
                zth::fail("bits = " + std::to_string(bits) + ", hashes = " + std::to_string(k) + ": the filter differs from the model");
                return;
            }
            const auto back = BloomFilter::from_bytes(f.to_bytes());
            if (!back || back->to_bytes() != f.to_bytes() || back->bits_set() != f.bits_set()) {
                zth::fail("bits = " + std::to_string(bits) + ": serialization does not round-trip");
                return;
            }
        }
    }
}
ZTH_TEST("robust", "random filters: add, merge, save and load agree with the model") {
    zth::Lcg rng(2025);
    for (int round = 0; round < 150; ++round) {
        const std::size_t bits = static_cast<std::size_t>(1 + rng.below(3000));
        const std::size_t k = static_cast<std::size_t>(1 + rng.below(8));
        BloomFilter a(bits, k), b(bits, k);
        Model ma(bits, k), mb(bits, k);
        const int na = rng.below(200), nb = rng.below(200);
        for (int i = 0; i < na; ++i) {
            const std::string item = "a" + std::to_string(rng.below(100000));
            a.add(item);
            ma.add(item);
        }
        for (int i = 0; i < nb; ++i) {
            const std::string item = "b" + std::to_string(rng.below(100000));
            b.add(item);
            mb.add(item);
        }
        if (a.to_bytes() != ma.bytes() || b.to_bytes() != mb.bytes()) {
            zth::fail("round " + std::to_string(round) + ": add disagrees with the model");
            return;
        }
        ZTH_EXPECT_TRUE(a.merge(b));
        for (std::size_t i = 0; i < bits; ++i) ma.set[i] = ma.set[i] || mb.set[i];
        if (a.to_bytes() != ma.bytes() || a.bits_set() != ma.count()) {
            zth::fail("round " + std::to_string(round) + ": merge disagrees with the model");
            return;
        }
        const auto loaded = BloomFilter::from_bytes(a.to_bytes());
        if (!loaded || loaded->to_bytes() != a.to_bytes() || loaded->bits_set() != a.bits_set()) {
            zth::fail("round " + std::to_string(round) + ": save/load disagrees");
            return;
        }
    }
}
ZTH_TEST("robust", "hostile input: headers that promise more data than there is are refused, whatever the numbers") {
    auto header = [](std::uint64_t bits, std::uint32_t hashes, std::size_t body) {
        std::vector<std::uint8_t> v;
        for (std::size_t i = 0; i < 8; ++i) v.push_back(static_cast<std::uint8_t>((bits >> (8 * i)) & 0xFF));
        for (std::size_t i = 0; i < 4; ++i) v.push_back(static_cast<std::uint8_t>((hashes >> (8 * i)) & 0xFF));
        v.insert(v.end(), body, std::uint8_t{0});
        return v;
    };
    ZTH_EXPECT_FALSE(BloomFilter::from_bytes(header(~0ULL, 3, 0)).has_value());            // 2^64 - 1 bits and no data (bits + 7 overflows)
    ZTH_EXPECT_FALSE(BloomFilter::from_bytes(header(~0ULL - 3, 3, 4)).has_value());
    ZTH_EXPECT_FALSE(BloomFilter::from_bytes(header(1ULL << 62, 3, 16)).has_value());
    ZTH_EXPECT_FALSE(BloomFilter::from_bytes(header(1ULL << 40, 1, 100)).has_value());
    ZTH_EXPECT_FALSE(BloomFilter::from_bytes(header(800, 2, 99)).has_value());              // needs 100 bytes
    ZTH_EXPECT_FALSE(BloomFilter::from_bytes(header(800, 2, 101)).has_value());
    ZTH_EXPECT_TRUE(BloomFilter::from_bytes(header(800, 2, 100)).has_value());
    ZTH_EXPECT_TRUE(BloomFilter::from_bytes(header(1, 1, 1)).has_value());
}

// ================================================ Fast ========================================================
ZTH_TEST("fast", "add and might_contain allocate nothing, even for long items") {
    BloomFilter f(100000, 7);
    const std::string long_item = "an item that is longer than any small string buffer could possibly hold, number ";
    std::vector<std::string> items;
    for (int i = 0; i < 100; ++i) items.push_back(long_item + std::to_string(i));
    zth::mem::Scope scope;
    std::size_t hits = 0;
    for (int round = 0; round < 500; ++round)
        for (const std::string& s : items) {
            f.add(s);
            hits += f.might_contain(s);
        }
    ZTH_EXPECT_ALLOCS_AT_MOST(scope, 0);
    ZTH_EXPECT_EQ(hits, std::size_t{50000});
}
ZTH_TEST_TIMEOUT("fast", "bits_set and fill_ratio are O(1): 200000 adds with a statistics call after each, on a 2-million-bit filter", 20) {
    BloomFilter f(2000000, 5);
    double sum = 0;
    for (int i = 0; i < 200000; ++i) {
        f.add(member(i));
        sum += f.fill_ratio() + static_cast<double>(f.bits_set());
    }
    ZTH_EXPECT_TRUE(sum > 0);
    ZTH_EXPECT_TRUE(f.bits_set() > 700000);
}
ZTH_TEST_TIMEOUT("fast", "three million adds and three million queries on an 8-million-bit filter", 20) {
    BloomFilter f(8000000, 5);
    for (int i = 0; i < 3000000; ++i) f.add("k" + std::to_string(i));
    std::size_t missing = 0;
    for (int i = 0; i < 3000000; ++i) missing += !f.might_contain("k" + std::to_string(i));
    ZTH_EXPECT_EQ(missing, std::size_t{0});
}
ZTH_TEST("fast", "the filter is compact: bits/8 bytes plus a few words, and to_bytes allocates its result once") {
    BloomFilter f = BloomFilter::for_capacity(1000000, 0.01);
    ZTH_EXPECT_EQ(f.bits(), std::size_t{9585059});
    zth::mem::Scope scope;
    const auto bytes = f.to_bytes();
    ZTH_EXPECT_ALLOCS_AT_MOST(scope, 1);
    ZTH_EXPECT_EQ(bytes.size(), std::size_t{12 + (9585059 + 7) / 8});
}
ZTH_TEST("fast", "a hostile header is refused without allocating anything") {
    std::vector<std::uint8_t> v(12, 0xFF);   // 2^64 - 1 bits and 2^32 - 1 hashes, no data
    zth::mem::Scope scope;
    const auto r = BloomFilter::from_bytes(v);
    ZTH_EXPECT_ALLOCS_AT_MOST(scope, 0);
    ZTH_EXPECT_FALSE(r.has_value());
}
ZTH_TEST_TIMEOUT("fast", "merging two big filters is one pass over the words", 20) {
    BloomFilter a(64000000, 3), b(64000000, 3);
    for (int i = 0; i < 1000; ++i) {
        a.add(member(i));
        b.add(stranger(i));
    }
    for (int i = 0; i < 5; ++i) ZTH_EXPECT_TRUE(a.merge(b));
    ZTH_EXPECT_TRUE(a.might_contain(stranger(7)));
}

// ================================================ Clean =======================================================
ZTH_TEST("clean", "no raw memory, no exceptions, no goto") {
    ZTH_EXPECT_NO_RAW_MEMORY("bloom.hpp");
    const std::string code = ::zth::strip_comments_and_literals(::zth::read_file("bloom.hpp"));
    for (const char* word : {"throw", "try", "catch", "goto"}) ZTH_EXPECT_TRUE(::zth::find_word(code, word).empty());
}
ZTH_TEST("clean", "a real bit array: no std::bitset (fixed size), no vector<bool>, no std::set of positions") {
    const std::string code = ::zth::strip_comments_and_literals(::zth::read_file("bloom.hpp"));
    for (const char* word : {"bitset", "unordered_set", "map", "unordered_map", "rand", "srand"}) ZTH_EXPECT_TRUE(::zth::find_word(code, word).empty());
    ZTH_EXPECT_TRUE(code.find("vector<bool>") == std::string::npos);
    ZTH_EXPECT_FALSE(::zth::find_word(code, "hash_pair").empty());
}
ZTH_TEST("clean", "bit counting uses std::popcount") {
    const std::string code = ::zth::strip_comments_and_literals(::zth::read_file("bloom.hpp"));
    ZTH_EXPECT_FALSE(::zth::find_word(code, "popcount").empty());
    ZTH_EXPECT_TRUE(::zth::find_word(code, "__builtin_popcount").empty());
    ZTH_EXPECT_TRUE(::zth::find_word(code, "__builtin_popcountll").empty());
}
