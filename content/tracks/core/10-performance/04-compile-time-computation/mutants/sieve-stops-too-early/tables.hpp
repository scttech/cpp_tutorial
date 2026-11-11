// MUTANT: p * p < N: the square of the last prime is never crossed out.
#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

constexpr unsigned long long factorial(unsigned n) {
    if (n > 20) return 0;  // 21! does not fit in 64 bits
    unsigned long long result = 1;
    for (unsigned i = 2; i <= n; ++i) result *= i;
    return result;
}

consteval std::uint32_t fnv1a(const char* text) {
    std::uint32_t hash = 2166136261u;
    for (; *text != '\0'; ++text) {
        hash ^= static_cast<unsigned char>(*text);  // an unsigned byte: a negative char would be sign-extended
        hash *= 16777619u;
    }
    return hash;
}

constexpr std::array<std::uint8_t, 256> make_popcount_table() {
    std::array<std::uint8_t, 256> table{};  // all zero: table[0] = 0
    for (unsigned i = 1; i < 256; ++i) table[i] = static_cast<std::uint8_t>(table[i / 2] + (i & 1));
    return table;
}

inline constexpr std::array<std::uint8_t, 256> popcount_table = make_popcount_table();

constexpr int popcount(std::uint32_t v) {
    return popcount_table[v & 0xFF] + popcount_table[(v >> 8) & 0xFF] + popcount_table[(v >> 16) & 0xFF] + popcount_table[v >> 24];
}

template <std::size_t N>
constexpr std::array<bool, N + 1> make_sieve() {
    std::array<bool, N + 1> prime{};
    for (std::size_t i = 2; i <= N; ++i) prime[i] = true;
    for (std::size_t p = 2; p * p < N; ++p) {
        if (!prime[p]) continue;
        for (std::size_t multiple = p * p; multiple <= N; multiple += p) prime[multiple] = false;
    }
    return prime;
}

inline constinit unsigned g_calls = static_cast<unsigned>(factorial(5));

inline unsigned next_call() { return ++g_calls; }
