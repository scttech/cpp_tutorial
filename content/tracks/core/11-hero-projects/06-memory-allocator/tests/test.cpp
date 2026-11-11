#define ZTH_TRACK_ALLOCATIONS
#include <algorithm>
#include <cstring>
#include <vector>

#include "allocator.hpp"
#include "zth_test.hpp"

using namespace alloc;

namespace {

constexpr unsigned char kCanary = 0xCD;

// A heap with canary bytes around it (and in the slack when it starts at an odd address): the allocator must never touch them.
struct Arena {
    std::vector<std::byte> raw;
    std::size_t region_start, region_size;
    Heap heap;
    Allocator a;
    explicit Arena(std::size_t size, std::size_t misalign = 0)
        : raw(size + 256, std::byte{kCanary}), region_start(64 + misalign), region_size(size), heap(raw.data() + region_start, region_size), a(heap) {}

    bool canaries_intact() const {
        const std::size_t begin = static_cast<std::size_t>(heap.base() - raw.data());
        const std::size_t end = begin + heap.size();
        for (std::size_t i = 0; i < raw.size(); ++i) {
            if ((i < begin || i >= end) && raw[i] != std::byte{kCanary}) return false;
        }
        return true;
    }
    std::size_t offset_of(const void* p) const { return static_cast<std::size_t>(static_cast<const std::byte*>(p) - heap.base()); }
};

struct Live {
    void* p;
    std::size_t n;
    unsigned char tag;
};

void fill(void* p, std::size_t n, unsigned char tag) { std::memset(p, tag, n); }
bool intact(const Live& b) {
    const unsigned char* c = static_cast<const unsigned char*>(b.p);
    for (std::size_t i = 0; i < b.n; ++i) {
        if (c[i] != b.tag) return false;
    }
    return true;
}

// the live blocks are inside the heap, aligned, and do not overlap
std::string layout_problem(const Arena& arena, std::vector<Live> live) {
    std::sort(live.begin(), live.end(), [](const Live& x, const Live& y) { return x.p < y.p; });
    for (std::size_t i = 0; i < live.size(); ++i) {
        const auto addr = reinterpret_cast<std::uintptr_t>(live[i].p);
        if (addr % 16 != 0) return "a pointer is not 16-byte aligned";
        if (!arena.heap.contains(live[i].p) || !arena.heap.contains(static_cast<unsigned char*>(live[i].p) + (live[i].n ? live[i].n - 1 : 0))) return "a block is outside the heap";
        if (i + 1 < live.size() && static_cast<unsigned char*>(live[i].p) + live[i].n > static_cast<unsigned char*>(live[i + 1].p)) return "two blocks overlap";
    }
    return "";
}

void expect_whole_again(Arena& arena) {
    const Stats s = arena.a.stats();
    if (s.used_blocks != 0) ::zth::fail("blocks are still in use: " + std::to_string(s.used_blocks));
    else if (s.free_blocks != 1) ::zth::fail("after freeing everything there should be ONE free block, there are " + std::to_string(s.free_blocks));
    else if (s.largest_free + 64 < arena.heap.size()) ::zth::fail("after freeing everything the largest free block is only " + std::to_string(s.largest_free) + " of " + std::to_string(arena.heap.size()));
    else if (!arena.a.check()) ::zth::fail("check() fails after freeing everything");
    else if (!arena.canaries_intact()) ::zth::fail("bytes outside the heap were overwritten");
}

}  // namespace

// ================================================ Works =======================================================
ZTH_TEST("works", "a new allocator: one big free block") {
    Arena arena(4096);
    const Stats s = arena.a.stats();
    ZTH_EXPECT_EQ(s.arena_size, arena.heap.size());
    ZTH_EXPECT_EQ(s.used_blocks, std::size_t{0});
    ZTH_EXPECT_EQ(s.used_bytes, std::size_t{0});
    ZTH_EXPECT_EQ(s.free_blocks, std::size_t{1});
    ZTH_EXPECT_TRUE(s.largest_free + 64 >= arena.heap.size());
    ZTH_EXPECT_TRUE(s.largest_free <= arena.heap.size());
    ZTH_EXPECT_TRUE(arena.a.check());
}
ZTH_TEST("works", "allocate gives aligned memory inside the heap; blocks do not overlap and keep what is written") {
    Arena arena(8192);
    std::vector<Live> live;
    const std::size_t sizes[] = {1, 7, 16, 17, 31, 32, 33, 100, 255, 256, 1000};
    unsigned char tag = 1;
    for (std::size_t n : sizes) {
        void* p = arena.a.allocate(n);
        ZTH_EXPECT_TRUE(p != nullptr);
        fill(p, n, tag);
        live.push_back({p, n, tag++});
    }
    ZTH_EXPECT_EQ(layout_problem(arena, live), std::string());
    for (const Live& b : live) ZTH_EXPECT_TRUE(intact(b));
    ZTH_EXPECT_EQ(arena.a.stats().used_blocks, live.size());
    ZTH_EXPECT_TRUE(arena.a.check());
    ZTH_EXPECT_TRUE(arena.canaries_intact());
}
ZTH_TEST("works", "usable_size is at least what was asked and not much more; used_bytes adds them up") {
    Arena arena(8192);
    std::size_t asked = 0, got = 0;
    for (std::size_t n : {1, 15, 16, 17, 40, 100, 333}) {
        void* p = arena.a.allocate(n);
        ZTH_EXPECT_TRUE(p != nullptr);
        const std::size_t usable = arena.a.usable_size(p);
        ZTH_EXPECT_TRUE(usable >= n);
        ZTH_EXPECT_TRUE(usable <= n + 47);
        asked += n;
        got += usable;
    }
    ZTH_EXPECT_EQ(arena.a.stats().used_bytes, got);
    ZTH_EXPECT_TRUE(got >= asked);
}
ZTH_TEST("works", "deallocate returns the memory: the same request can be made again") {
    Arena arena(1024);
    for (int round = 0; round < 5; ++round) {
        void* p = arena.a.allocate(800);
        ZTH_EXPECT_TRUE(p != nullptr);
        fill(p, 800, 7);
        arena.a.deallocate(p);
        ZTH_EXPECT_EQ(arena.a.stats().used_blocks, std::size_t{0});
    }
    expect_whole_again(arena);
}
ZTH_TEST("works", "allocate(0) gives a valid, distinct, freeable block") {
    Arena arena(1024);
    void* a = arena.a.allocate(0);
    void* b = arena.a.allocate(0);
    ZTH_EXPECT_TRUE(a != nullptr);
    ZTH_EXPECT_TRUE(b != nullptr);
    ZTH_EXPECT_TRUE(a != b);
    ZTH_EXPECT_EQ(arena.a.stats().used_blocks, std::size_t{2});
    arena.a.deallocate(a);
    arena.a.deallocate(b);
    expect_whole_again(arena);
}
ZTH_TEST("works", "a request that does not fit returns nullptr and counts as failed; the heap is unchanged") {
    Arena arena(1024);
    const Stats before = arena.a.stats();
    ZTH_EXPECT_TRUE(arena.a.allocate(1024) == nullptr);
    ZTH_EXPECT_TRUE(arena.a.allocate(100000) == nullptr);
    const Stats after = arena.a.stats();
    ZTH_EXPECT_EQ(after.failed_allocations, std::size_t{2});
    ZTH_EXPECT_EQ(after.used_blocks, before.used_blocks);
    ZTH_EXPECT_EQ(after.free_blocks, before.free_blocks);
    ZTH_EXPECT_EQ(after.largest_free, before.largest_free);
    ZTH_EXPECT_TRUE(arena.a.check());
}
ZTH_TEST("works", "the largest free block can be allocated, and then nothing is left") {
    Arena arena(2048);
    const std::size_t all = arena.a.stats().largest_free;
    void* p = arena.a.allocate(all);
    ZTH_EXPECT_TRUE(p != nullptr);
    fill(p, all, 9);
    ZTH_EXPECT_TRUE(arena.a.allocate(1) == nullptr);
    ZTH_EXPECT_EQ(arena.a.stats().largest_free, std::size_t{0});
    arena.a.deallocate(p);
    expect_whole_again(arena);
    ZTH_EXPECT_TRUE(arena.canaries_intact());
}
ZTH_TEST("works", "splitting: a small block does not use up a big free block") {
    Arena arena(65536);
    void* p = arena.a.allocate(100);
    ZTH_EXPECT_TRUE(p != nullptr);
    const Stats s = arena.a.stats();
    ZTH_EXPECT_TRUE(s.largest_free + 100 + 128 >= arena.heap.size());
    ZTH_EXPECT_EQ(s.free_blocks, std::size_t{1});
}
ZTH_TEST("works", "allocate until the heap is full, free everything in order: one free block again") {
    for (int order = 0; order < 3; ++order) {
        Arena arena(16384);
        std::vector<void*> blocks;
        while (void* p = arena.a.allocate(64)) {
            fill(p, 64, static_cast<unsigned char>(blocks.size()));
            blocks.push_back(p);
        }
        ZTH_EXPECT_TRUE(blocks.size() >= 16384 / (64 + 32));
        ZTH_EXPECT_TRUE(arena.a.check());
        if (order == 1) std::reverse(blocks.begin(), blocks.end());
        if (order == 2) {
            ::zth::Lcg rng(7);
            for (std::size_t i = blocks.size(); i > 1; --i) std::swap(blocks[i - 1], blocks[static_cast<std::size_t>(rng.below(static_cast<int>(i)))]);
        }
        for (void* p : blocks) arena.a.deallocate(p);
        expect_whole_again(arena);
    }
}
ZTH_TEST("works", "coalescing: A B C freed as A C B (or any order) become one block") {
    Arena arena(4096);
    void* a = arena.a.allocate(100);
    void* b = arena.a.allocate(100);
    void* c = arena.a.allocate(100);
    void* d = arena.a.allocate(100);  // keeps the others away from the big tail block
    arena.a.deallocate(a);
    ZTH_EXPECT_EQ(arena.a.stats().free_blocks, std::size_t{2});
    arena.a.deallocate(c);
    ZTH_EXPECT_EQ(arena.a.stats().free_blocks, std::size_t{3});
    arena.a.deallocate(b);  // merges with both neighbours
    ZTH_EXPECT_EQ(arena.a.stats().free_blocks, std::size_t{2});
    ZTH_EXPECT_TRUE(arena.a.stats().largest_free >= 300);
    ZTH_EXPECT_TRUE(arena.a.check());
    arena.a.deallocate(d);
    expect_whole_again(arena);
}
ZTH_TEST("works", "coalescing with only the next block, and with only the previous block") {
    Arena arena(4096);
    void* a = arena.a.allocate(64);
    void* b = arena.a.allocate(64);
    void* c = arena.a.allocate(64);
    void* d = arena.a.allocate(64);
    arena.a.deallocate(b);
    arena.a.deallocate(a);  // a merges with the free b behind it
    ZTH_EXPECT_EQ(arena.a.stats().free_blocks, std::size_t{2});
    ZTH_EXPECT_TRUE(arena.a.stats().largest_free >= 2 * 64);
    arena.a.deallocate(c);  // c merges with the free block before it
    ZTH_EXPECT_EQ(arena.a.stats().free_blocks, std::size_t{2});
    ZTH_EXPECT_TRUE(arena.a.stats().largest_free >= 3 * 64);
    arena.a.deallocate(d);
    expect_whole_again(arena);
}
ZTH_TEST("works", "a freed hole is reused before the heap runs out") {
    Arena arena(2048);
    std::vector<void*> blocks;
    while (void* p = arena.a.allocate(200)) blocks.push_back(p);
    ZTH_EXPECT_TRUE(blocks.size() >= 7);
    ZTH_EXPECT_TRUE(arena.a.allocate(200) == nullptr);
    arena.a.deallocate(blocks[3]);
    void* again = arena.a.allocate(200);
    ZTH_EXPECT_TRUE(again != nullptr);
    ZTH_EXPECT_TRUE(arena.a.allocate(200) == nullptr);
}
ZTH_TEST("works", "reallocate: grow keeps the contents, shrink keeps the prefix, nullptr acts as allocate, 0 acts as free") {
    Arena arena(8192);
    void* p = arena.a.reallocate(nullptr, 50);
    ZTH_EXPECT_TRUE(p != nullptr);
    fill(p, 50, 0x5A);
    p = arena.a.reallocate(p, 500);
    ZTH_EXPECT_TRUE(p != nullptr);
    ZTH_EXPECT_TRUE(arena.a.usable_size(p) >= 500);
    ZTH_EXPECT_TRUE(intact({p, 50, 0x5A}));
    fill(p, 500, 0x6B);
    p = arena.a.reallocate(p, 20);
    ZTH_EXPECT_TRUE(p != nullptr);
    ZTH_EXPECT_TRUE(intact({p, 20, 0x6B}));
    ZTH_EXPECT_TRUE(arena.a.reallocate(p, 0) == nullptr);
    expect_whole_again(arena);
}
ZTH_TEST("works", "reallocate moves the block when the neighbour is in the way, and the old block is freed") {
    Arena arena(8192);
    void* a = arena.a.allocate(100);
    void* b = arena.a.allocate(100);
    fill(a, 100, 0x11);
    fill(b, 100, 0x22);
    void* a2 = arena.a.reallocate(a, 1000);
    ZTH_EXPECT_TRUE(a2 != nullptr);
    ZTH_EXPECT_TRUE(intact({a2, 100, 0x11}));
    ZTH_EXPECT_TRUE(intact({b, 100, 0x22}));
    ZTH_EXPECT_EQ(arena.a.stats().used_blocks, std::size_t{2});
    arena.a.deallocate(a2);
    arena.a.deallocate(b);
    expect_whole_again(arena);
}
ZTH_TEST("works", "stats count allocations, deallocations, failures") {
    Arena arena(1024);
    void* a = arena.a.allocate(10);
    void* b = arena.a.allocate(10);
    (void)arena.a.allocate(5000);
    arena.a.deallocate(a);
    arena.a.deallocate(b);
    arena.a.deallocate(nullptr);
    const Stats s = arena.a.stats();
    ZTH_EXPECT_EQ(s.allocations, std::size_t{2});
    ZTH_EXPECT_EQ(s.deallocations, std::size_t{2});
    ZTH_EXPECT_EQ(s.failed_allocations, std::size_t{1});
    ZTH_EXPECT_EQ(s.rejected_frees, std::size_t{0});
}

// ================================================ Robust ======================================================
ZTH_TEST("robust", "deallocate(nullptr) is fine and not counted; a pointer outside the heap is rejected") {
    Arena arena(2048);
    void* p = arena.a.allocate(100);
    arena.a.deallocate(nullptr);
    ZTH_EXPECT_EQ(arena.a.stats().rejected_frees, std::size_t{0});
    int on_the_stack = 5;
    arena.a.deallocate(&on_the_stack);
    arena.a.deallocate(arena.heap.base() - 16);
    arena.a.deallocate(arena.heap.base() + arena.heap.size());
    arena.a.deallocate(arena.heap.base() + arena.heap.size() + 64);
    ZTH_EXPECT_EQ(arena.a.stats().rejected_frees, std::size_t{4});
    ZTH_EXPECT_EQ(arena.a.stats().used_blocks, std::size_t{1});
    ZTH_EXPECT_TRUE(arena.a.check());
    ZTH_EXPECT_TRUE(arena.canaries_intact());
    arena.a.deallocate(p);
    expect_whole_again(arena);
}
ZTH_TEST("robust", "a pointer into the middle of a block, or to the start of the heap, is rejected (the payload holds ordinary data)") {
    Arena arena(2048);
    void* p = arena.a.allocate(256);
    void* q = arena.a.allocate(256);
    fill(p, 256, 0xAB);
    fill(q, 256, 0xAB);
    unsigned char* b = static_cast<unsigned char*>(p);
    arena.a.deallocate(b + 1);
    arena.a.deallocate(b + 16);
    arena.a.deallocate(b + 32);
    arena.a.deallocate(b + 128);
    arena.a.deallocate(arena.heap.base());
    arena.a.deallocate(arena.heap.base() + 8);
    ZTH_EXPECT_EQ(arena.a.stats().rejected_frees, std::size_t{6});
    ZTH_EXPECT_EQ(arena.a.usable_size(nullptr), std::size_t{0});
    ZTH_EXPECT_EQ(arena.a.usable_size(b + 1), std::size_t{0});
    ZTH_EXPECT_EQ(arena.a.usable_size(b + 128), std::size_t{0});
    int outside = 0;
    ZTH_EXPECT_EQ(arena.a.usable_size(&outside), std::size_t{0});
    ZTH_EXPECT_EQ(arena.a.stats().used_blocks, std::size_t{2});
    ZTH_EXPECT_TRUE(intact({p, 256, 0xAB}));
    ZTH_EXPECT_TRUE(intact({q, 256, 0xAB}));
    ZTH_EXPECT_TRUE(arena.a.check());
    arena.a.deallocate(p);
    ZTH_EXPECT_EQ(arena.a.usable_size(p), std::size_t{0});  // freed: no longer a live block
    arena.a.deallocate(q);
    expect_whole_again(arena);
}
ZTH_TEST("robust", "a double free is rejected, whether the block is alone, merged with its neighbours, or reused") {
    Arena arena(4096);
    void* a = arena.a.allocate(64);
    void* b = arena.a.allocate(64);
    void* c = arena.a.allocate(64);
    arena.a.deallocate(b);
    arena.a.deallocate(b);  // alone, between used blocks
    ZTH_EXPECT_EQ(arena.a.stats().rejected_frees, std::size_t{1});
    arena.a.deallocate(a);  // a and b merge
    arena.a.deallocate(a);
    arena.a.deallocate(b);
    ZTH_EXPECT_EQ(arena.a.stats().rejected_frees, std::size_t{3});
    arena.a.deallocate(c);  // everything merges
    arena.a.deallocate(c);
    ZTH_EXPECT_EQ(arena.a.stats().rejected_frees, std::size_t{4});
    expect_whole_again(arena);
    void* x = arena.a.allocate(64);
    void* y = arena.a.allocate(64);
    ZTH_EXPECT_TRUE(x != nullptr && y != nullptr && x != y);
    fill(x, 64, 1);
    fill(y, 64, 2);
    ZTH_EXPECT_TRUE(intact({x, 64, 1}));
    arena.a.deallocate(x);
    arena.a.deallocate(x);
    ZTH_EXPECT_TRUE(intact({y, 64, 2}));
    ZTH_EXPECT_TRUE(arena.a.check());
    ZTH_EXPECT_EQ(arena.a.stats().used_blocks, std::size_t{1});
}
ZTH_TEST("robust", "after rejected frees the allocator still hands out disjoint blocks") {
    Arena arena(8192);
    std::vector<Live> live;
    for (int i = 0; i < 20; ++i) {
        void* p = arena.a.allocate(100 + static_cast<std::size_t>(i));
        fill(p, 100 + static_cast<std::size_t>(i), static_cast<unsigned char>(i + 1));
        live.push_back({p, 100 + static_cast<std::size_t>(i), static_cast<unsigned char>(i + 1)});
    }
    void* freed = live[5].p;
    arena.a.deallocate(freed);
    live.erase(live.begin() + 5);
    for (int k = 0; k < 10; ++k) {
        arena.a.deallocate(freed);
        arena.a.deallocate(static_cast<unsigned char*>(live[3].p) + 8);
    }
    for (int i = 0; i < 10; ++i) {
        void* p = arena.a.allocate(90);
        ZTH_EXPECT_TRUE(p != nullptr);
        fill(p, 90, static_cast<unsigned char>(100 + i));
        live.push_back({p, 90, static_cast<unsigned char>(100 + i)});
    }
    ZTH_EXPECT_EQ(layout_problem(arena, live), std::string());
    for (const Live& b : live) ZTH_EXPECT_TRUE(intact(b));
    ZTH_EXPECT_TRUE(arena.a.check());
}
ZTH_TEST("robust", "huge requests fail cleanly (no overflow in the rounding)") {
    Arena arena(2048);
    const std::size_t max = static_cast<std::size_t>(-1);
    for (std::size_t n : {max, max - 1, max - 8, max - 15, max - 16, max - 31, max - 47, max / 2, max / 2 + 1, std::size_t{1} << 40}) {
        ZTH_EXPECT_TRUE(arena.a.allocate(n) == nullptr);
    }
    void* p = arena.a.allocate(10);
    ZTH_EXPECT_TRUE(p != nullptr);
    ZTH_EXPECT_TRUE(arena.a.reallocate(p, max) == nullptr);
    ZTH_EXPECT_TRUE(arena.a.reallocate(p, max - 20) == nullptr);
    fill(p, 10, 3);
    ZTH_EXPECT_TRUE(intact({p, 10, 3}));  // the old block is still valid after the failed reallocate
    ZTH_EXPECT_EQ(arena.a.stats().used_blocks, std::size_t{1});
    ZTH_EXPECT_TRUE(arena.a.check());
    ZTH_EXPECT_TRUE(arena.canaries_intact());
}
ZTH_TEST("robust", "a heap that starts at an odd address, has an odd size, or is tiny") {
    for (std::size_t misalign : {1, 3, 8, 15}) {
        Arena arena(4093, misalign);
        void* p = arena.a.allocate(100);
        ZTH_EXPECT_TRUE(p != nullptr);
        ZTH_EXPECT_EQ(reinterpret_cast<std::uintptr_t>(p) % 16, std::uintptr_t{0});
        fill(p, 100, 4);
        arena.a.deallocate(p);
        expect_whole_again(arena);
    }
    for (std::size_t size : {0, 1, 15, 16, 31, 32, 33, 47, 48, 63, 64}) {
        Arena arena(size, 5);
        ZTH_EXPECT_TRUE(arena.a.check());
        std::vector<void*> blocks;
        while (void* p = arena.a.allocate(1)) {
            blocks.push_back(p);
            if (blocks.size() > 10) break;
        }
        ZTH_EXPECT_TRUE(blocks.size() <= 3);
        for (void* p : blocks) arena.a.deallocate(p);
        ZTH_EXPECT_TRUE(arena.a.check());
        ZTH_EXPECT_TRUE(arena.canaries_intact());
    }
}
ZTH_TEST("robust", "reallocate: failure keeps the old block and its contents; bad pointers; same size; nullptr and zero") {
    Arena arena(1024);
    void* a = arena.a.allocate(300);
    void* b = arena.a.allocate(300);
    fill(a, 300, 0x31);
    fill(b, 300, 0x32);
    ZTH_EXPECT_TRUE(arena.a.reallocate(a, 700) == nullptr);  // does not fit anywhere
    ZTH_EXPECT_TRUE(intact({a, 300, 0x31}));
    ZTH_EXPECT_TRUE(intact({b, 300, 0x32}));
    ZTH_EXPECT_EQ(arena.a.stats().used_blocks, std::size_t{2});
    void* same = arena.a.reallocate(a, 300);
    ZTH_EXPECT_TRUE(same != nullptr);
    ZTH_EXPECT_TRUE(intact({same, 300, 0x31}));
    int x = 0;
    ZTH_EXPECT_TRUE(arena.a.reallocate(&x, 10) == nullptr);
    ZTH_EXPECT_TRUE(arena.a.reallocate(static_cast<unsigned char*>(b) + 16, 10) == nullptr);
    ZTH_EXPECT_EQ(arena.a.stats().rejected_frees, std::size_t{2});
    ZTH_EXPECT_TRUE(arena.a.reallocate(nullptr, 0) != nullptr);  // allocate(0): a valid block
    ZTH_EXPECT_TRUE(arena.a.check());
}
ZTH_TEST("robust", "reallocate in place: growing into a free neighbour and shrinking keep the pointer and the data") {
    Arena arena(8192);
    void* a = arena.a.allocate(100);
    fill(a, 100, 0x41);
    void* grown = arena.a.reallocate(a, 2000);  // the rest of the heap is free behind it
    ZTH_EXPECT_TRUE(grown != nullptr);
    ZTH_EXPECT_TRUE(intact({grown, 100, 0x41}));
    ZTH_EXPECT_TRUE(arena.a.usable_size(grown) >= 2000);
    ZTH_EXPECT_EQ(arena.a.stats().used_blocks, std::size_t{1});
    ZTH_EXPECT_EQ(arena.a.stats().free_blocks, std::size_t{1});
    void* small = arena.a.reallocate(grown, 40);
    ZTH_EXPECT_TRUE(small == grown);  // shrinking never moves
    ZTH_EXPECT_TRUE(arena.a.stats().largest_free + 128 >= arena.heap.size() - 40 - 64);
    ZTH_EXPECT_EQ(arena.a.stats().free_blocks, std::size_t{1});
    ZTH_EXPECT_TRUE(arena.a.check());
    arena.a.deallocate(small);
    expect_whole_again(arena);
}
ZTH_TEST("robust", "fragmentation: free every other block, then the rest") {
    Arena arena(32768);
    std::vector<void*> blocks;
    while (void* p = arena.a.allocate(48)) blocks.push_back(p);
    const std::size_t n = blocks.size();
    ZTH_EXPECT_TRUE(n > 300);
    for (std::size_t i = 0; i < n; i += 2) arena.a.deallocate(blocks[i]);
    const Stats half = arena.a.stats();
    ZTH_EXPECT_TRUE(half.free_blocks >= n / 2);
    ZTH_EXPECT_TRUE(half.largest_free < 200);  // only small holes
    ZTH_EXPECT_TRUE(arena.a.allocate(200) == nullptr);
    ZTH_EXPECT_TRUE(arena.a.check());
    for (std::size_t i = 1; i < n; i += 2) arena.a.deallocate(blocks[i]);
    expect_whole_again(arena);
}
ZTH_TEST("robust", "the allocator never changes the contents of blocks in use, even on freshly freed neighbours") {
    Arena arena(16384);
    std::vector<Live> live;
    for (int i = 0; i < 60; ++i) {
        const std::size_t n = 20 + static_cast<std::size_t>(i % 7) * 37;
        void* p = arena.a.allocate(n);
        fill(p, n, static_cast<unsigned char>(i + 1));
        live.push_back({p, n, static_cast<unsigned char>(i + 1)});
    }
    for (int i = 0; i < 60; i += 3) {  // free some, which writes bookkeeping into those blocks only
        arena.a.deallocate(live[static_cast<std::size_t>(i)].p);
        live[static_cast<std::size_t>(i)].p = nullptr;
    }
    for (const Live& b : live) {
        if (b.p) ZTH_EXPECT_TRUE(intact(b));
    }
    ZTH_EXPECT_TRUE(arena.canaries_intact());
}
ZTH_TEST("robust", "check() notices a damaged heap (its first block header overwritten with 0xFF bytes, or with zeros)") {
    for (const unsigned char fill_byte : {static_cast<unsigned char>(0xFF), static_cast<unsigned char>(0x00)}) {
        Arena arena(4096);
        void* a = arena.a.allocate(100);
        void* b = arena.a.allocate(100);
        (void)b;
        ZTH_EXPECT_TRUE(arena.a.check());
        std::memset(arena.heap.base(), fill_byte, 48);
        ZTH_EXPECT_FALSE(arena.a.check());
        (void)a;
    }
}
ZTH_TEST_TIMEOUT("robust", "stress: 30000 random allocate / deallocate / reallocate operations with patterns, overlap checks and check()", 20) {
    Arena arena(262144);
    ::zth::Lcg rng(2024);
    std::vector<Live> live;
    unsigned char tag = 1;
    auto size = [&]() -> std::size_t {
        switch (rng.below(10)) {
            case 0: return static_cast<std::size_t>(1000 + rng.below(3000));
            case 1: case 2: case 3: return static_cast<std::size_t>(65 + rng.below(450));
            default: return static_cast<std::size_t>(1 + rng.below(64));
        }
    };
    for (int step = 0; step < 30000; ++step) {
        const int action = rng.below(100);
        if (action < 45 || live.empty()) {
            const std::size_t n = size();
            void* p = arena.a.allocate(n);
            if (p) {
                fill(p, n, tag);
                live.push_back({p, n, tag});
                tag = static_cast<unsigned char>(tag % 250 + 1);
            }
        } else if (action < 85) {
            const std::size_t i = static_cast<std::size_t>(rng.below(static_cast<int>(live.size())));
            if (!intact(live[i])) {
                ::zth::fail("a block was modified behind my back (step " + std::to_string(step) + ")");
                return;
            }
            arena.a.deallocate(live[i].p);
            live[i] = live.back();
            live.pop_back();
        } else {
            const std::size_t i = static_cast<std::size_t>(rng.below(static_cast<int>(live.size())));
            const std::size_t n = size();
            const std::size_t keep = std::min(n, live[i].n);
            void* q = arena.a.reallocate(live[i].p, n);
            if (q) {
                if (!intact({q, keep, live[i].tag})) {
                    ::zth::fail("reallocate lost the contents (step " + std::to_string(step) + ")");
                    return;
                }
                fill(q, n, live[i].tag);
                live[i].p = q;
                live[i].n = n;
            } else if (!intact(live[i])) {
                ::zth::fail("a failed reallocate damaged the old block (step " + std::to_string(step) + ")");
                return;
            }
        }
        if (step % 500 == 0) {
            if (!arena.a.check()) {
                ::zth::fail("check() failed at step " + std::to_string(step));
                return;
            }
            const std::string problem = layout_problem(arena, live);
            if (!problem.empty()) {
                ::zth::fail(problem + " (step " + std::to_string(step) + ")");
                return;
            }
            if (arena.a.stats().used_blocks != live.size()) {
                ::zth::fail("used_blocks is " + std::to_string(arena.a.stats().used_blocks) + ", " + std::to_string(live.size()) + " blocks are live (step " + std::to_string(step) + ")");
                return;
            }
        }
    }
    for (const Live& b : live) {
        if (!intact(b)) {
            ::zth::fail("a block was modified behind my back (at the end)");
            return;
        }
        arena.a.deallocate(b.p);
    }
    expect_whole_again(arena);
}

// ================================================ Fast ========================================================
namespace {
// average touches per operation of: allocate n blocks of mixed sizes, then free them all in random order
void touches_per_op(std::size_t n, double& per_alloc, double& per_free) {
    Arena arena(n * 160 + 4096);
    ::zth::Lcg rng(5);
    std::vector<void*> blocks;
    blocks.reserve(n);
    stats_counter::reset();
    for (std::size_t i = 0; i < n; ++i) blocks.push_back(arena.a.allocate(16 + static_cast<std::size_t>(rng.below(120))));
    per_alloc = static_cast<double>(stats_counter::touches) / static_cast<double>(n);
    for (std::size_t i = blocks.size(); i > 1; --i) std::swap(blocks[i - 1], blocks[static_cast<std::size_t>(rng.below(static_cast<int>(i)))]);
    stats_counter::reset();
    for (void* p : blocks) arena.a.deallocate(p);
    per_free = static_cast<double>(stats_counter::touches) / static_cast<double>(n);
}
}  // namespace

ZTH_TEST_TIMEOUT("fast", "allocate and deallocate take a constant number of heap accesses: at most 12 per allocate and 22 per deallocate (20000 blocks, frees in random order)", 20) {
    double alloc_cost = 0, free_cost = 0;
    touches_per_op(20000, alloc_cost, free_cost);
    ZTH_EXPECT_TRUE(alloc_cost <= 12.0);
    ZTH_EXPECT_TRUE(free_cost <= 22.0);
}
ZTH_TEST_TIMEOUT("fast", "the cost per operation does not grow with the number of blocks (5000 vs 20000 blocks)", 20) {
    double a1 = 0, f1 = 0, a2 = 0, f2 = 0;
    touches_per_op(5000, a1, f1);
    touches_per_op(20000, a2, f2);
    ZTH_EXPECT_TRUE(a1 > 0 && f1 > 0);
    ZTH_EXPECT_TRUE(a2 <= a1 * 1.3 + 1);
    ZTH_EXPECT_TRUE(f2 <= f1 * 1.3 + 1);
}
ZTH_TEST_TIMEOUT("fast", "steady-state churn: 5000 live blocks, 100000 free+allocate pairs, at most 40 heap accesses per pair", 20) {
    Arena arena(2 * 1024 * 1024);
    ::zth::Lcg rng(77);
    std::vector<void*> live;
    for (int i = 0; i < 5000; ++i) live.push_back(arena.a.allocate(16 + static_cast<std::size_t>(rng.below(200))));
    stats_counter::reset();
    for (int i = 0; i < 100000; ++i) {
        const std::size_t k = static_cast<std::size_t>(rng.below(5000));
        arena.a.deallocate(live[k]);
        live[k] = arena.a.allocate(16 + static_cast<std::size_t>(rng.below(200)));
        if (!live[k]) {
            ::zth::fail("the heap ran out in the churn test");
            return;
        }
    }
    ZTH_EXPECT_TRUE(stats_counter::touches <= 100000ULL * 40);
}
ZTH_TEST_TIMEOUT("fast", "the allocator uses no memory of its own: not one call to new / malloc during 20000 operations", 20) {
    Arena arena(1024 * 1024);
    std::vector<void*> blocks(2000);
    ::zth::Lcg rng(3);
    zth::mem::Scope scope;
    for (int round = 0; round < 10; ++round) {
        for (auto& p : blocks) p = arena.a.allocate(8 + static_cast<std::size_t>(rng.below(300)));
        for (auto& p : blocks) {
            arena.a.deallocate(p);
            p = nullptr;
        }
    }
    (void)arena.a.stats();
    ZTH_EXPECT_ALLOCS_AT_MOST(scope, 0);
}
ZTH_TEST_TIMEOUT("fast", "overhead per block is at most 32 bytes: 16-byte blocks fill a 64 KB heap with at least 1300 blocks", 20) {
    Arena arena(65536);
    std::size_t count = 0;
    while (arena.a.allocate(16)) ++count;
    ZTH_EXPECT_TRUE(count >= 1300);
}
ZTH_TEST_TIMEOUT("fast", "splitting does not leave unusable slivers: after allocating 17-byte blocks the free space is still one block", 20) {
    Arena arena(65536);
    for (int i = 0; i < 100; ++i) ZTH_EXPECT_TRUE(arena.a.allocate(17) != nullptr);
    const Stats s = arena.a.stats();
    ZTH_EXPECT_EQ(s.free_blocks, std::size_t{1});
    ZTH_EXPECT_TRUE(s.used_bytes <= 100 * 17 + 100 * 31);
}
ZTH_TEST_TIMEOUT("fast", "400000 operations in time (no walk over all the blocks, no sorted insertion)", 20) {
    Arena arena(8 * 1024 * 1024);
    ::zth::Lcg rng(11);
    std::vector<void*> live(60000, nullptr);
    for (int i = 0; i < 400000; ++i) {
        void*& slot = live[static_cast<std::size_t>(rng.below(60000))];
        if (slot) {
            arena.a.deallocate(slot);
            slot = nullptr;
        } else {
            slot = arena.a.allocate(8 + static_cast<std::size_t>(rng.below(100)));
        }
    }
    ZTH_EXPECT_TRUE(arena.a.check());
}

// ================================================ Clean =======================================================
ZTH_TEST("clean", "no new / delete / malloc / free anywhere: the allocator is the memory manager") {
    ZTH_EXPECT_NO_RAW_MEMORY("blocks.hpp");
    ZTH_EXPECT_NO_RAW_MEMORY("allocator.hpp");
}
ZTH_TEST("clean", "all memory is reached through Heap::at: no reinterpret_cast, no C-style pointer casts to header types") {
    for (const char* file : {"blocks.hpp", "allocator.hpp"}) {
        const std::string code = ::zth::strip_comments_and_literals(::zth::read_file(file));
        for (const char* word : {"reinterpret_cast", "const_cast", "goto", "throw", "try", "catch", "alloca", "calloc", "malloc"}) {
            if (!::zth::find_word(code, word).empty()) {
                ::zth::fail(std::string(file) + " uses `" + word + "`");
                return;
            }
        }
    }
}
ZTH_TEST("clean", "no containers and no smart pointers: an allocator cannot use the standard library's allocations") {
    for (const char* file : {"blocks.hpp", "allocator.hpp"}) {
        const std::string code = ::zth::strip_comments_and_literals(::zth::read_file(file));
        for (const char* word : {"vector", "map", "set", "list", "deque", "string", "unique_ptr", "shared_ptr", "function", "make_unique", "make_shared", "unordered_map", "array"}) {
            if (!::zth::find_word(code, word).empty()) {
                ::zth::fail(std::string(file) + " uses `" + word + "`");
                return;
            }
        }
    }
}
ZTH_TEST("clean", "the bookkeeping types are named: a Header struct and a Links (or similar) struct in blocks.hpp, used through Heap::at<>") {
    const std::string blocks = ::zth::strip_comments_and_literals(::zth::read_file("blocks.hpp"));
    ZTH_EXPECT_TRUE(!::zth::find_word(blocks, "struct").empty());
    const std::string code = ::zth::strip_comments_and_literals(::zth::read_file("allocator.hpp"));
    ZTH_EXPECT_TRUE(code.find(".at<") != std::string::npos || code.find("->at<") != std::string::npos);
}
