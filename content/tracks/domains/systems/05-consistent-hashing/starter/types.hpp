#pragma once
// READ-ONLY. The hash function and the naming of virtual nodes, so that every implementation puts the same points on the same ring.
#include <cstdint>
#include <string>
#include <string_view>

namespace chash {

// 64-bit FNV-1a followed by the splitmix64 finalizer (FNV alone spreads similar strings such as "node-1#7" and "node-1#8" badly).
inline std::uint64_t hash64(std::string_view text) {
    std::uint64_t h = 14695981039346656037ULL;
    for (char c : text) {
        h ^= static_cast<unsigned char>(c);
        h *= 1099511628211ULL;
    }
    h ^= h >> 30;
    h *= 0xbf58476d1ce4e5b9ULL;
    h ^= h >> 27;
    h *= 0x94d049bb133111ebULL;
    h ^= h >> 31;
    return h;
}

// The label that is hashed to place virtual node number `index` of `node` on the ring:  "<node>#<index>".
inline std::string vnode_label(std::string_view node, std::size_t index) {
    std::string label(node);
    label += '#';
    label += std::to_string(index);
    return label;
}

}  // namespace chash
