#pragma once
// YOUR BLOCK LAYOUT: what a block looks like inside the heap, and the small helpers to read it.
//
// A suggested design (boundary tags + a doubly linked free list); you may choose another, the tests only see the Allocator API.
//   every block starts with a 16-byte Header { size | in-use flag, size of the physically previous block };
//   the usable bytes (payload) follow the header; a FREE block keeps two list links (offsets) in its payload;
//   all sizes are multiples of 16, the smallest block is 32 (a header and 16 bytes).
// All memory is reached through Heap::at<T>(offset): never cast a pointer yourself.
#include <cstddef>

#include "types.hpp"

namespace alloc {

inline constexpr std::size_t kAlign = 16;
inline constexpr std::size_t kNil = static_cast<std::size_t>(-1);

struct Header {
    std::size_t size_flags;  // TODO: total size of the block (multiple of 16), bit 0 set when the block is in use
    std::size_t prev_size;   // TODO: total size of the physically previous block (0 for the first block)
};
inline constexpr std::size_t kHeader = sizeof(Header);
inline constexpr std::size_t kMinBlock = 2 * kAlign;

struct Links {
    std::size_t next;  // TODO: offsets of the neighbours in the list of free blocks (kNil: none)
    std::size_t prev;
};

inline std::size_t round_up(std::size_t n) { return (n + kAlign - 1) & ~(kAlign - 1); }

}  // namespace alloc
