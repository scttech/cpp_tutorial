#pragma once
// READ-ONLY. The hash of an item, as two independent 64-bit values for double hashing.
#include <cstdint>
#include <string_view>

namespace bloom {

struct HashPair {
    std::uint64_t h1;
    std::uint64_t h2;
};

namespace detail {
inline std::uint64_t fnv1a(std::string_view text, std::uint64_t seed) {
    std::uint64_t h = seed;
    for (char c : text) {
        h ^= static_cast<unsigned char>(c);
        h *= 1099511628211ULL;
    }
    return h;
}
inline std::uint64_t finalize(std::uint64_t h) {  // splitmix64 finalizer
    h ^= h >> 30;
    h *= 0xbf58476d1ce4e5b9ULL;
    h ^= h >> 27;
    h *= 0x94d049bb133111ebULL;
    h ^= h >> 31;
    return h;
}
}  // namespace detail

// Two different hashes of the same bytes (different FNV seeds). Bit number i of an item is  (h1 + i * h2) mod bits  (Kirsch-Mitzenmacher double hashing).
inline HashPair hash_pair(std::string_view item) {
    return HashPair{detail::finalize(detail::fnv1a(item, 14695981039346656037ULL)), detail::finalize(detail::fnv1a(item, 0x9e3779b97f4a7c15ULL))};
}

}  // namespace bloom
