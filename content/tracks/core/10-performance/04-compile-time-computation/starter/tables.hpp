#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

// Work done by the compiler costs nothing at run time. Fill in the five pieces below; the comments say what each must do.

// ---- 1. constexpr function -------------------------------------------------------------------------------------------------------------
// factorial(n) = 1 * 2 * ... * n, with factorial(0) = 1. It must work in a constant expression (constexpr auto x = factorial(10);) AND at run time.
// 20! is the largest that fits in an unsigned long long: for n > 20 return 0.
constexpr unsigned long long factorial(unsigned n) {
    (void)n;
    return 0;
}

// ---- 2. consteval function -------------------------------------------------------------------------------------------------------------
// fnv1a(text): the 32-bit FNV-1a hash of the characters of a C string, so that switch (hash) { case fnv1a("GET"): ... } works.
//   hash = 2166136261;  for each character c:  hash = hash XOR (c as an UNSIGNED byte);  hash = hash * 16777619;     (all in std::uint32_t)
// It is `consteval`: it may only be called with arguments that are known at compile time, and always runs inside the compiler.
consteval std::uint32_t fnv1a(const char* text) {
    return static_cast<unsigned char>(text[0]);  // (a stub: only the first character, so that the case labels below stay different)
}

// ---- 3. a lookup table built at compile time -------------------------------------------------------------------------------------------
// make_popcount_table()[i] = how many bits of i are 1, for i in 0..255. popcount_table is computed while compiling (constexpr);
// popcount(v) = the number of 1 bits in a 32-bit value, as four table lookups.
constexpr std::array<std::uint8_t, 256> make_popcount_table() { return {}; }

inline constexpr std::array<std::uint8_t, 256> popcount_table = make_popcount_table();

constexpr int popcount(std::uint32_t v) {
    (void)v;
    return 0;
}

// ---- 4. a sieve of Eratosthenes at compile time ----------------------------------------------------------------------------------------
// make_sieve<N>()[i] is true exactly when i is a prime number, for i in 0..N (0 and 1 are not prime). The compiler gives up on a constant
// expression that takes too many steps (about a million): checking every number by trial division up to N = 30000 is too slow, a sieve is not.
template <std::size_t N>
constexpr std::array<bool, N + 1> make_sieve() {
    return {};
}

// ---- 5. constinit ----------------------------------------------------------------------------------------------------------------------
// g_calls starts as factorial(5) and counts calls of next_call(). It is NOT const (next_call changes it), but `constinit` makes the compiler
// check that it is initialised at compile time, so no code runs before main() and no other global's constructor can see it half-built.
inline unsigned g_calls = 0;

inline unsigned next_call() { return ++g_calls; }
