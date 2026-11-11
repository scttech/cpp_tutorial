// The compile-time tables at work (read-only). Press Run.
#include <cstdint>
#include <iostream>

#include "tables.hpp"

// consteval: fnv1a("...") is computed by the COMPILER, so it can be a case label.
const char* route(std::uint32_t hash) {
    switch (hash) {
        case fnv1a("GET"): return "read";
        case fnv1a("POST"): return "create";
        case fnv1a("DELETE"): return "remove";
        default: return "unknown";
    }
}

int main() {
    constexpr auto big = factorial(15);  // computed while compiling
    std::cout << "15! = " << big << "\n";
    std::cout << "hash of \"hello\" = " << fnv1a("hello") << "\n";
    std::cout << "bits in 0xF0F0 = " << popcount(0xF0F0u) << "\n";
    constexpr auto sieve = make_sieve<100>();
    int primes = 0;
    for (bool is_prime : sieve) primes += is_prime ? 1 : 0;
    std::cout << "primes up to 100: " << primes << "\n";
    std::cout << "route(\"POST\") = " << route(fnv1a("POST")) << "\n";
    std::cout << "calls so far: " << next_call() << "\n";
}
