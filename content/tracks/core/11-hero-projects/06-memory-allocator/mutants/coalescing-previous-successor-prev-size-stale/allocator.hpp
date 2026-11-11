// MUTANT: after merging with the predecessor the following block is not updated
#pragma once
// A first-fit allocator with boundary tags. Free blocks form a doubly linked LIFO list (insertion and removal in O(1));
// freeing looks at the physical neighbours through the sizes in the headers and merges them in O(1).
#include <cstddef>
#include <cstring>

#include "blocks.hpp"
#include "types.hpp"

namespace alloc {

class Allocator {
public:
    explicit Allocator(Heap& heap) : heap_(heap) {
        if (heap_.size() >= kMinBlock) {
            Header& h = hdr(0);
            h.size_flags = heap_.size();
            h.prev_size = 0;
            link_free(0);
        }
    }

    void* allocate(std::size_t bytes) {
        if (bytes > static_cast<std::size_t>(-1) - 4 * kAlign) {  // rounding up would overflow
            ++failed_;
            return nullptr;
        }
        const std::size_t need = block_for(bytes);
        for (std::size_t off = free_head_; off != kNil; off = links(off).next) {
            if (block_size(hdr(off)) >= need) {
                take(off, need);
                ++allocations_;
                return payload(off);
            }
        }
        ++failed_;
        return nullptr;
    }

    void deallocate(void* p) {
        if (p == nullptr) return;
        const std::size_t off = validate(p);
        if (off == kNil) {
            ++rejected_;
            return;
        }
        release(off);
        ++deallocations_;
    }

    void* reallocate(void* p, std::size_t bytes) {
        if (p == nullptr) return allocate(bytes);
        const std::size_t off = validate(p);
        if (off == kNil) {
            ++rejected_;
            return nullptr;
        }
        if (bytes == 0) {
            release(off);
            ++deallocations_;
            return nullptr;
        }
        if (bytes > static_cast<std::size_t>(-1) - 4 * kAlign) {
            ++failed_;
            return nullptr;
        }
        const std::size_t need = block_for(bytes);
        const std::size_t current = block_size(hdr(off));
        if (need <= current) {  // shrink (or keep): give the tail back
            shrink(off, need);
            return p;
        }
        const std::size_t after = off + current;  // grow in place into a free neighbour?
        if (after < heap_.size() && !in_use(hdr(after)) && current + block_size(hdr(after)) >= need) {
            const std::size_t merged = current + block_size(hdr(after));
            unlink_free(after);
            used_bytes_ += merged - current;
            hdr(off).size_flags = merged | 1;
            if (off + merged < heap_.size()) hdr(off + merged).prev_size = merged;
            shrink(off, need);
            return p;
        }
        void* q = allocate(bytes);  // move
        if (q == nullptr) return nullptr;
        std::memcpy(q, p, current - kHeader);
        deallocate(p);
        return q;
    }

    std::size_t usable_size(const void* p) const {
        const std::size_t off = validate(p);
        return off == kNil ? 0 : block_size(hdr(off)) - kHeader;
    }

    Stats stats() const {
        Stats s;
        s.arena_size = heap_.size();
        s.used_blocks = used_blocks_;
        s.used_bytes = used_bytes_;
        s.free_blocks = free_blocks_;
        s.free_bytes = free_bytes_;
        for (std::size_t off = free_head_; off != kNil; off = links(off).next) {
            const std::size_t usable = block_size(hdr(off)) - kHeader;
            if (usable > s.largest_free) s.largest_free = usable;
        }
        s.allocations = allocations_;
        s.deallocations = deallocations_;
        s.failed_allocations = failed_;
        s.rejected_frees = rejected_;
        return s;
    }

    // walk the blocks in address order and the free list; compare with the counters
    bool check() const {
        if (heap_.size() < kMinBlock) return used_blocks_ == 0 && free_blocks_ == 0;  // too small for any block
        std::size_t off = 0, prev_size = 0, used = 0, free_count = 0, used_cap = 0, free_cap = 0;
        bool previous_free = false;
        while (off < heap_.size()) {
            if (heap_.size() - off < kMinBlock) return false;
            const Header& h = hdr(off);
            const std::size_t size = block_size(h);
            if (size < kMinBlock || size % kAlign != 0 || size > heap_.size() - off || h.prev_size != prev_size) return false;
            if (in_use(h)) {
                ++used;
                used_cap += size - kHeader;
                previous_free = false;
            } else {
                if (previous_free) return false;  // two free neighbours must have been merged
                ++free_count;
                free_cap += size - kHeader;
                previous_free = true;
            }
            prev_size = size;
            off += size;
        }
        if (used != used_blocks_ || free_count != free_blocks_ || used_cap != used_bytes_ || free_cap != free_bytes_) return false;
        std::size_t listed = 0, before = kNil;
        for (std::size_t f = free_head_; f != kNil; f = links(f).next) {
            if (f >= heap_.size() || in_use(hdr(f)) || links(f).prev != before || ++listed > free_blocks_) return false;
            before = f;
        }
        return listed == free_blocks_;
    }

private:
    Heap& heap_;
    std::size_t free_head_ = kNil;
    std::size_t used_blocks_ = 0, used_bytes_ = 0, free_blocks_ = 0, free_bytes_ = 0;
    std::size_t allocations_ = 0, deallocations_ = 0, failed_ = 0, rejected_ = 0;

    Header& hdr(std::size_t off) const { return heap_.at<Header>(off); }
    Links& links(std::size_t off) const { return heap_.at<Links>(off + kHeader); }
    void* payload(std::size_t off) const { return heap_.base() + off + kHeader; }

    static std::size_t block_for(std::size_t bytes) { return kHeader + (bytes < kAlign ? kAlign : round_up(bytes)); }

    void link_free(std::size_t off) {  // push on the front of the free list
        Links& l = links(off);
        l.prev = kNil;
        l.next = free_head_;
        if (free_head_ != kNil) links(free_head_).prev = off;
        free_head_ = off;
        ++free_blocks_;
        free_bytes_ += block_size(hdr(off)) - kHeader;
    }
    void unlink_free(std::size_t off) {
        const Links l = links(off);
        if (l.prev != kNil) links(l.prev).next = l.next;
        else free_head_ = l.next;
        if (l.next != kNil) links(l.next).prev = l.prev;
        --free_blocks_;
        free_bytes_ -= block_size(hdr(off)) - kHeader;
    }

    // the free block at `off` becomes a block of `need` bytes in use (the rest, if big enough, stays free)
    void take(std::size_t off, std::size_t need) {
        const std::size_t size = block_size(hdr(off));
        unlink_free(off);
        hdr(off).size_flags = size | 1;
        ++used_blocks_;
        used_bytes_ += size - kHeader;
        shrink(off, need);
    }

    // the block in use at `off` keeps only `need` bytes; the tail (if at least a minimal block) becomes a free block, merged with a free successor
    void shrink(std::size_t off, std::size_t need) {
        const std::size_t size = block_size(hdr(off));
        if (size - need < kMinBlock) return;
        hdr(off).size_flags = need | 1;
        used_bytes_ -= size - need;
        const std::size_t tail = off + need;
        Header& t = hdr(tail);
        t.size_flags = size - need;
        t.prev_size = need;
        if (tail + (size - need) < heap_.size()) hdr(tail + (size - need)).prev_size = size - need;
        merge_and_link(tail);
    }

    // the free (not yet listed) block at `off` is merged with a free successor and then listed
    void merge_and_link(std::size_t off) {
        std::size_t size = block_size(hdr(off));
        const std::size_t next = off + size;
        if (next < heap_.size() && !in_use(hdr(next))) {
            unlink_free(next);
            size += block_size(hdr(next));
            hdr(off).size_flags = size;
            if (off + size < heap_.size()) hdr(off + size).prev_size = size;
        }
        link_free(off);
    }

    // free the block in use at `off`: merge with the free neighbours (found through the sizes in the headers)
    void release(std::size_t off) {
        const std::size_t size = block_size(hdr(off));
        --used_blocks_;
        used_bytes_ -= size - kHeader;
        hdr(off).size_flags = size;  // not in use any more
        const std::size_t before = hdr(off).prev_size;
        if (before != 0 && !in_use(hdr(off - before))) {
            const std::size_t prev = off - before;
            unlink_free(prev);
            const std::size_t merged = block_size(hdr(prev)) + size;
            hdr(prev).size_flags = merged;
            off = prev;
        }
        merge_and_link(off);
    }

    // the header offset of the live block whose payload starts at p; kNil if p is not that
    std::size_t validate(const void* p) const {
        if (p == nullptr || !heap_.contains(p)) return kNil;
        const std::size_t at = static_cast<std::size_t>(static_cast<const std::byte*>(p) - heap_.base());
        if (at < kHeader || at % kAlign != 0) return kNil;
        const std::size_t off = at - kHeader;
        const Header& h = hdr(off);
        if (!in_use(h)) return kNil;
        const std::size_t size = block_size(h);
        if (size < kMinBlock || size % kAlign != 0 || size > heap_.size() - off) return kNil;
        if (off + size < heap_.size() && hdr(off + size).prev_size != size) return kNil;  // the successor agrees on my size
        if (off != 0) {                                                                    // so does the predecessor
            if (h.prev_size < kMinBlock || h.prev_size > off || h.prev_size % kAlign != 0) return kNil;
            if (block_size(hdr(off - h.prev_size)) != h.prev_size) return kNil;
        } else if (h.prev_size != 0) {
            return kNil;
        }
        return off;
    }
};

}  // namespace alloc
