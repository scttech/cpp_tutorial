#include <cstdint>
#include <cstdio>
#include <string>
#include <vector>

#include "tables.hpp"
#include "zth_test.hpp"

namespace {

// The same hash, written at run time, for comparison.
std::uint32_t ref_fnv(const std::string& s) {
    std::uint32_t h = 2166136261u;
    for (unsigned char c : s) {
        h ^= c;
        h *= 16777619u;
    }
    return h;
}

// These are evaluated by the COMPILER. If a function is not constexpr (or goes wrong in a constant expression) the program does not even compile.
constexpr auto kFact0 = factorial(0);
constexpr auto kFact5 = factorial(5);
constexpr auto kFact10 = factorial(10);
constexpr auto kFact20 = factorial(20);
constexpr auto kFact21 = factorial(21);
constexpr auto kHashEmpty = fnv1a("");
constexpr auto kHashA = fnv1a("a");
constexpr auto kHashHello = fnv1a("hello");
constexpr auto kHashHigh = fnv1a("caf\xC3\xA9");
constexpr int kBits = popcount(0xF0F0F0F0u);
constexpr auto kSieve100 = make_sieve<100>();
constexpr auto kSieve49 = make_sieve<49>();
constexpr auto kSieve97 = make_sieve<97>();
constexpr auto kSieve30000 = make_sieve<30000>();  // the compiler stops at about a million steps: trial division does not get there

int route(std::uint32_t h) {
    switch (h) {
        case fnv1a("GET"): return 1;
        case fnv1a("POST"): return 2;
        case fnv1a("DELETE"): return 3;
        default: return 0;
    }
}

}  // namespace

// ================================================ Works =======================================================
ZTH_TEST("works", "factorial: 0! = 1, 5! = 120, 10! = 3628800 (computed by the compiler)") {
    ZTH_EXPECT_EQ(kFact0, 1ULL);
    ZTH_EXPECT_EQ(kFact5, 120ULL);
    ZTH_EXPECT_EQ(kFact10, 3628800ULL);
    ZTH_EXPECT_EQ(factorial(12), 479001600ULL);  // and at run time
}
ZTH_TEST("works", "fnv1a: the hash of \"\" is 2166136261, of \"a\" 0xE40C292C, of \"hello\" 0x4F9F2CAB") {
    ZTH_EXPECT_EQ(kHashEmpty, 2166136261u);
    ZTH_EXPECT_EQ(kHashA, 0xE40C292Cu);
    ZTH_EXPECT_EQ(kHashHello, 0x4F9F2CABu);
}
ZTH_TEST("works", "fnv1a can be a case label: route() picks the right branch") {
    ZTH_EXPECT_EQ(route(ref_fnv("GET")), 1);
    ZTH_EXPECT_EQ(route(ref_fnv("POST")), 2);
    ZTH_EXPECT_EQ(route(ref_fnv("DELETE")), 3);
    ZTH_EXPECT_EQ(route(ref_fnv("PATCH")), 0);
}
ZTH_TEST("works", "the popcount table: a few entries, and popcount() of a few values") {
    ZTH_EXPECT_EQ(static_cast<int>(popcount_table[0]), 0);
    ZTH_EXPECT_EQ(static_cast<int>(popcount_table[1]), 1);
    ZTH_EXPECT_EQ(static_cast<int>(popcount_table[22]), 3);  // 0b10110
    ZTH_EXPECT_EQ(static_cast<int>(popcount_table[255]), 8);
    ZTH_EXPECT_EQ(kBits, 16);
    ZTH_EXPECT_EQ(popcount(0), 0);
    ZTH_EXPECT_EQ(popcount(0xFFFFFFFFu), 32);
}
ZTH_TEST("works", "the sieve: the primes up to 100") {
    int count = 0;
    for (bool p : kSieve100) count += p ? 1 : 0;
    ZTH_EXPECT_EQ(count, 25);
    ZTH_EXPECT_TRUE(kSieve100[2] && kSieve100[3] && kSieve100[97]);
    ZTH_EXPECT_FALSE(kSieve100[4] || kSieve100[91] || kSieve100[100]);
}
ZTH_TEST("works", "next_call() counts up from 120 + 1 (g_calls starts as 5!)") {
    const unsigned a = next_call();
    const unsigned b = next_call();
    ZTH_EXPECT_TRUE(a >= 121);
    ZTH_EXPECT_EQ(b, a + 1);
}

// ================================================ Robust ======================================================
ZTH_TEST("robust", "factorial: 20! = 2432902008176640000 and anything bigger is 0 (no wrap-around)") {
    ZTH_EXPECT_EQ(kFact20, 2432902008176640000ULL);
    ZTH_EXPECT_EQ(kFact21, 0ULL);
    ZTH_EXPECT_EQ(factorial(100), 0ULL);
    ZTH_EXPECT_EQ(factorial(1), 1ULL);
    ZTH_EXPECT_EQ(factorial(2), 2ULL);
}
ZTH_TEST("robust", "fnv1a treats every character as an UNSIGNED byte (non-ASCII text hashes like the reference)") {
    ZTH_EXPECT_EQ(kHashHigh, ref_fnv("caf\xC3\xA9"));
    ZTH_EXPECT_EQ(fnv1a("\xFF\xFE"), ref_fnv("\xFF\xFE"));
}
ZTH_TEST("robust", "every entry of the popcount table is right, and so is popcount() of powers of two and of random values") {
    for (unsigned i = 0; i < 256; ++i) ZTH_EXPECT_EQ(static_cast<int>(popcount_table[i]), __builtin_popcount(i));
    for (unsigned b = 0; b < 32; ++b) ZTH_EXPECT_EQ(popcount(1u << b), 1);
    ::zth::Lcg rng(11);
    for (int i = 0; i < 200; ++i) {
        const std::uint32_t v = static_cast<std::uint32_t>(rng.next());
        ZTH_EXPECT_EQ(popcount(v), __builtin_popcount(v));
    }
}
ZTH_TEST("robust", "the sieve: 0 and 1 are not prime, the sizes are right, and it agrees with trial division for every number up to 2000") {
    ZTH_EXPECT_FALSE(kSieve100[0]);
    ZTH_EXPECT_FALSE(kSieve100[1]);
    ZTH_EXPECT_EQ(kSieve100.size(), std::size_t{101});
    ZTH_EXPECT_EQ(kSieve30000.size(), std::size_t{30001});
    for (unsigned n = 0; n <= 2000; ++n) {
        bool prime = n >= 2;
        for (unsigned d = 2; d * d <= n && prime; ++d) prime = n % d != 0;
        ZTH_EXPECT_EQ(kSieve30000[n], prime);
    }
}
ZTH_TEST("robust", "the sieve: the last entries (the end of the array is used)") {
    ZTH_EXPECT_TRUE(kSieve30000[29989]);   // the largest prime below 30000
    ZTH_EXPECT_FALSE(kSieve30000[30000]);
    ZTH_EXPECT_FALSE(kSieve30000[29990]);
}
ZTH_TEST("robust", "the sieve: N itself is included when it is prime (97), and a prime squared is crossed out (49)") {
    ZTH_EXPECT_TRUE(kSieve97[97]);
    ZTH_EXPECT_EQ(kSieve97.size(), std::size_t{98});
    ZTH_EXPECT_TRUE(kSieve49[47]);
    ZTH_EXPECT_FALSE(kSieve49[49]);
}

// ================================================ Fast ========================================================
ZTH_TEST("fast", "a sieve for 30000 numbers is built by the compiler within its step limit: there are 3245 primes below 30000") {
    int count = 0;
    for (bool p : kSieve30000) count += p ? 1 : 0;
    ZTH_EXPECT_EQ(count, 3245);
}
ZTH_TEST("fast", "the popcount table is a constant: popcount() works in a constant expression") {
    ZTH_EXPECT_EQ(kBits, 16);
    constexpr int fromTable = popcount_table[200];
    ZTH_EXPECT_EQ(fromTable, 3);
}

// ================================================ Clean =======================================================
ZTH_TEST("clean", "fnv1a is declared consteval, not just constexpr") {
    const std::string code = ::zth::strip_comments_and_literals(::zth::read_file("tables.hpp"));
    ZTH_EXPECT_FALSE(::zth::find_word(code, "consteval").empty());
}
ZTH_TEST("clean", "g_calls is declared constinit") {
    const std::string code = ::zth::strip_comments_and_literals(::zth::read_file("tables.hpp"));
    ZTH_EXPECT_FALSE(::zth::find_word(code, "constinit").empty());
}
ZTH_TEST("clean", "no macros and no const_cast: the compiler does the work, not the preprocessor") {
    const std::string code = ::zth::strip_comments_and_literals(::zth::read_file("tables.hpp"));
    ZTH_EXPECT_TRUE(::zth::find_word(code, "define").empty());
    ZTH_EXPECT_TRUE(::zth::find_word(code, "const_cast").empty());
}
