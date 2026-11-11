// MUTANT: the minimum block is 96 bytes
#pragma once
// Block layout: a 16-byte header with the size (and the in-use flag) of the block and the size of its physical predecessor
// (a "boundary tag": it makes the previous block reachable in O(1)); free blocks keep list links in their payload.
#include <cstddef>

#include "types.hpp"

namespace alloc {

inline constexpr std::size_t kAlign = 16;
inline constexpr std::size_t kNil = static_cast<std::size_t>(-1);

struct Header {
    std::size_t size_flags;  // total size of the block (a multiple of 16) | 1 when in use
    std::size_t prev_size;   // total size of the physically previous block, 0 for the first one
};
inline constexpr std::size_t kHeader = sizeof(Header);   // 16: so the payload is aligned too
inline constexpr std::size_t kMinBlock = 6 * kAlign;     // a header and the 16 bytes that hold the two links of a free block

struct Links {
    std::size_t next;  // offsets of the neighbours in the list of free blocks
    std::size_t prev;
};

inline std::size_t round_up(std::size_t n) { return (n + kAlign - 1) & ~(kAlign - 1); }
inline std::size_t block_size(const Header& h) { return h.size_flags & ~static_cast<std::size_t>(1); }
inline bool in_use(const Header& h) { return (h.size_flags & 1) != 0; }

}  // namespace alloc
