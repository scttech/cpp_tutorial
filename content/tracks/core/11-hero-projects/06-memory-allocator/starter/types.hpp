#pragma once
// READ-ONLY. The memory your allocator manages (Heap), what it reports (Stats), and the counter that the Fast tests read.
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>

namespace alloc {

namespace stats_counter {
inline unsigned long long touches = 0;  // how many times the allocator looked at memory of the heap through Heap::at
inline void reset() { touches = 0; }
}  // namespace stats_counter

// A region of raw memory. The allocator keeps all its bookkeeping INSIDE it (in the blocks), addressed by byte OFFSETS from
// base(), and reads and writes that bookkeeping only through at<T>(offset). Every access is counted, checked to be inside the
// heap and aligned, and aborts with a message if not: that is how the tests (and you) find out about a wild pointer.
class Heap {
public:
    // `memory` may start at any address: the heap uses the 16-byte aligned part of it. The size is rounded down to a multiple of 16.
    Heap(void* memory, std::size_t size) {
        const std::uintptr_t start = reinterpret_cast<std::uintptr_t>(memory);
        const std::uintptr_t aligned = (start + 15) & ~static_cast<std::uintptr_t>(15);
        const std::size_t lost = static_cast<std::size_t>(aligned - start);
        base_ = reinterpret_cast<std::byte*>(aligned);
        size_ = size > lost ? (size - lost) & ~static_cast<std::size_t>(15) : 0;
    }

    std::byte* base() const { return base_; }
    std::size_t size() const { return size_; }  // a multiple of 16
    bool contains(const void* p) const {
        const std::byte* b = static_cast<const std::byte*>(p);
        return b >= base_ && b < base_ + size_;
    }

    // a T that lives at `offset` bytes from base(); T may need at most 16-byte alignment
    template <class T>
    T& at(std::size_t offset) const {
        static_assert(alignof(T) <= 16, "the heap is only 16-byte aligned");
        if (offset % alignof(T) != 0 || offset > size_ || sizeof(T) > size_ - offset) {
            std::fprintf(stderr, "heap access out of range or misaligned: offset %zu, size %zu, heap size %zu\n", offset, sizeof(T), size_);
            std::abort();
        }
        ++stats_counter::touches;
        return *reinterpret_cast<T*>(base_ + offset);
    }

private:
    std::byte* base_ = nullptr;
    std::size_t size_ = 0;
};

struct Stats {
    std::size_t arena_size = 0;       // the size of the heap (Heap::size())
    std::size_t used_blocks = 0;      // blocks handed out and not yet freed
    std::size_t used_bytes = 0;       // the usable bytes (capacities) of those blocks: at least what was asked
    std::size_t free_blocks = 0;      // separate free blocks (adjacent free blocks are one, after coalescing)
    std::size_t free_bytes = 0;       // the usable bytes (capacities) of the free blocks
    std::size_t largest_free = 0;     // the largest request that would succeed right now
    std::size_t allocations = 0;      // successful allocate() calls so far (a successful reallocate that moves counts as one more)
    std::size_t deallocations = 0;    // successful deallocate() calls so far (not counting nullptr, nor rejected pointers)
    std::size_t failed_allocations = 0;  // allocate() calls that returned nullptr
    std::size_t rejected_frees = 0;   // deallocate() / reallocate() calls with a pointer that is not a live block (nullptr is not counted)
};

}  // namespace alloc
