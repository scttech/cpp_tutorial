#pragma once
// YOUR ALLOCATOR: malloc, free and realloc on a fixed region of memory.
//
//   Allocator(Heap&)          takes charge of the heap: it is one big free block at first (or nothing, if it is too small for a block)
//   allocate(n)               a pointer to at least n usable bytes, aligned to 16, inside the heap; nullptr if no free block is big enough
//                             (allocate(0) gives a valid minimal block; a huge n such as SIZE_MAX must fail cleanly, not overflow)
//   deallocate(p)             nullptr: nothing. A live block: freed and merged with free neighbours. Anything else (outside the heap,
//                             not at the start of a block, already freed): rejected, counted in stats().rejected_frees, nothing changes.
//   reallocate(p, n)          like realloc: p == nullptr is allocate(n); n == 0 frees p and returns nullptr; otherwise a block of at
//                             least n bytes with the old contents (up to the smaller size). If that is impossible, nullptr and the old
//                             block stays valid. A bad p is rejected like in deallocate (returns nullptr).
//   usable_size(p)            the usable bytes of the live block p (0 if p is not a live block)
//   stats()                   see types.hpp
//   check()                   walk the whole heap and verify your own invariants; true if everything is consistent
#include <cstddef>
#include <cstring>

#include "blocks.hpp"
#include "types.hpp"

namespace alloc {

class Allocator {
public:
    explicit Allocator(Heap& heap) : heap_(heap) {
        // TODO: one free block that covers the whole heap
    }

    void* allocate(std::size_t bytes) {
        (void)bytes;
        ++failed_;
        return nullptr;  // TODO
    }

    void deallocate(void* p) { (void)p; }  // TODO

    void* reallocate(void* p, std::size_t bytes) {
        (void)p;
        (void)bytes;
        return nullptr;  // TODO
    }

    std::size_t usable_size(const void* p) const {
        (void)p;
        return 0;  // TODO
    }

    Stats stats() const {
        Stats s;
        s.arena_size = heap_.size();
        s.failed_allocations = failed_;
        return s;  // TODO: the rest
    }

    bool check() const { return true; }  // TODO

private:
    Heap& heap_;
    std::size_t failed_ = 0;
};

}  // namespace alloc
