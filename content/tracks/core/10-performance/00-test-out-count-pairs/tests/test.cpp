#include <algorithm>
#include <cstdio>
#include <string>
#include <type_traits>
#include <vector>

#include "count_pairs.hpp"
#include "probe.hpp"
#include "zth_test.hpp"

namespace {

// The obviously correct answer, for comparison.
long long brute(const std::vector<long long>& v, long long target) {
    long long pairs = 0;
    for (std::size_t i = 0; i < v.size(); ++i)
        for (std::size_t j = i + 1; j < v.size(); ++j)
            if (v[i] + v[j] == target) ++pairs;
    return pairs;
}

std::vector<Num> nums(const std::vector<long long>& v) { return std::vector<Num>(v.begin(), v.end()); }

std::vector<long long> random_values(std::size_t n, unsigned seed, long long range, long long shift = 0) {
    ::zth::Lcg rng(seed);
    std::vector<long long> v(n);
    for (auto& x : v) x = static_cast<long long>(rng.next() % static_cast<unsigned>(range)) - shift;
    return v;
}

// Operations (== < +) that count_pairs needs for n random numbers.
unsigned long long ops_for(std::size_t n) {
    const auto values = nums(random_values(n, 5, 1000));
    probe::reset();
    (void)count_pairs(values, Num(1000));
    return probe::ops();
}

}  // namespace

// ================================================ Works =======================================================
ZTH_TEST("works", "a few small cases: [1,2,3,4] with target 5 has two pairs") {
    ZTH_EXPECT_EQ(count_pairs(std::vector<int>{1, 2, 3, 4}, 5), 2LL);
    ZTH_EXPECT_EQ(count_pairs(std::vector<int>{4, 1, 3, 2}, 5), 2LL);
    ZTH_EXPECT_EQ(count_pairs(std::vector<int>{1, 2, 3}, 100), 0LL);
}
ZTH_TEST("works", "it agrees with the brute-force count on 300 random numbers (many equal values)") {
    for (unsigned seed = 1; seed <= 4; ++seed) {
        const auto v = random_values(300, seed, 50);
        for (long long target : {0LL, 25LL, 49LL, 60LL, 98LL}) ZTH_EXPECT_EQ(count_pairs(v, target), brute(v, target));
    }
}
ZTH_TEST("works", "the same answer for a user-defined number type (Num)") {
    const auto v = random_values(200, 9, 100);
    ZTH_EXPECT_EQ(count_pairs(nums(v), Num(100)), brute(v, 100));
}
ZTH_TEST("works", "the result type is long long") {
    static_assert(std::is_same_v<decltype(count_pairs(std::vector<int>{}, 0)), long long>);
    ZTH_EXPECT_TRUE(true);
}

// ================================================ Robust ======================================================
ZTH_TEST("robust", "an empty vector and a vector with one value have no pairs") {
    ZTH_EXPECT_EQ(count_pairs(std::vector<int>{}, 0), 0LL);
    ZTH_EXPECT_EQ(count_pairs(std::vector<int>{5}, 10), 0LL);
    ZTH_EXPECT_EQ(count_pairs(std::vector<int>{5}, 5), 0LL);
}
ZTH_TEST("robust", "a value is never paired with itself, but two equal values at different positions are a pair") {
    ZTH_EXPECT_EQ(count_pairs(std::vector<int>{3}, 6), 0LL);
    ZTH_EXPECT_EQ(count_pairs(std::vector<int>{3, 3}, 6), 1LL);
    ZTH_EXPECT_EQ(count_pairs(std::vector<int>{3, 3, 3}, 6), 3LL);
    ZTH_EXPECT_EQ(count_pairs(std::vector<int>(2000, 7), 14), 1999000LL);
}
ZTH_TEST("robust", "many copies of each of two values: the pairs are the product of the counts") {
    std::vector<int> v;
    for (int i = 0; i < 40; ++i) v.push_back(1);
    for (int i = 0; i < 25; ++i) v.push_back(9);
    ZTH_EXPECT_EQ(count_pairs(v, 10), 1000LL);
    ZTH_EXPECT_EQ(count_pairs(v, 2), 780LL);
    ZTH_EXPECT_EQ(count_pairs(v, 18), 300LL);
}
ZTH_TEST("robust", "negative values, zeros and a negative target") {
    const std::vector<int> v = {-5, 5, 0, 0, -10, 10, 3, -3, 7};
    ZTH_EXPECT_EQ(count_pairs(v, 0), 4LL);
    ZTH_EXPECT_EQ(count_pairs(v, -10), 2LL);
    ZTH_EXPECT_EQ(count_pairs(v, -20), 0LL);
}
ZTH_TEST("robust", "the caller's vector is not changed (not sorted, not touched)") {
    const std::vector<int> original = {9, 1, 8, 2, 7, 3, 6, 4, 5, 5};
    std::vector<int> v = original;
    ZTH_EXPECT_EQ(count_pairs(v, 10), 5LL);
    ZTH_EXPECT_TRUE(v == original);
}

// ================================================ Fast ========================================================
ZTH_TEST("fast", "2000 numbers take far fewer than a million operations (trying every pair takes four million)") {
    ZTH_EXPECT_TRUE(ops_for(2000) <= 150000ULL);
}
ZTH_TEST("fast", "twice the numbers cost about twice the work, not four times (n log n, not n squared)") {
    const auto a = ops_for(1000);
    const auto b = ops_for(2000);
    const auto c = ops_for(4000);
    ZTH_EXPECT_TRUE(a > 0);
    ZTH_EXPECT_TRUE(b <= a * 5 / 2);  // n log n doubles to ~2.2x; n squared gives 4x
    ZTH_EXPECT_TRUE(c <= b * 5 / 2);
}
ZTH_TEST("fast", "the numbers are copied a few times at most (copy the vector once, not in a loop)") {
    const auto values = nums(random_values(2000, 3, 1000));
    probe::reset();
    (void)count_pairs(values, Num(1000));
    ZTH_EXPECT_TRUE(probe::copies <= 3 * 2000ULL);
}
