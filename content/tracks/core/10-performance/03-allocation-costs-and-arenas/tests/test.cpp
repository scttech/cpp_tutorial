#define ZTH_TRACK_ALLOCATIONS
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <memory_resource>
#include <string>
#include <vector>

#include "arena.hpp"
#include "counting.hpp"
#include "zth_test.hpp"

namespace {

struct Pt {
    int x, y;
    Pt(int a, int b) : x(a), y(b) {}
};

template <class T>
constexpr bool can_create = requires(Arena& a) { a.template create<T>(); };
static_assert(can_create<int>, "create<int> must exist");
// (create<std::string> must be refused: an arena never runs destructors. Checked in a test below.)
static_assert(!std::is_copy_constructible_v<Arena>, "an Arena owns its block: it cannot be copied");

bool aligned_to(const void* p, std::size_t a) { return reinterpret_cast<std::uintptr_t>(p) % a == 0; }

}  // namespace

// ================================================ Works =======================================================
ZTH_TEST("works", "allocations are separate, writable regions") {
    CountingResource up;
    Arena a(4096, &up);
    unsigned char* p[10];
    for (int i = 0; i < 10; ++i) {
        p[i] = static_cast<unsigned char*>(a.allocate(24, 1));
        ZTH_EXPECT_TRUE(p[i] != nullptr);
        std::memset(p[i], 0x10 + i, 24);
    }
    for (int i = 0; i < 10; ++i)
        for (int j = 0; j < 24; ++j) ZTH_EXPECT_EQ(static_cast<int>(p[i][j]), 0x10 + i);
}
ZTH_TEST("works", "used() counts what was handed out, alignment padding included") {
    CountingResource up;
    Arena a(1024, &up);
    ZTH_EXPECT_EQ(a.used(), std::size_t{0});
    ZTH_EXPECT_EQ(a.capacity(), std::size_t{1024});
    (void)a.allocate(10, 1);
    ZTH_EXPECT_EQ(a.used(), std::size_t{10});
    void* p = a.allocate(4, 4);  // starts at offset 12: two bytes of padding
    ZTH_EXPECT_TRUE(aligned_to(p, 4));
    ZTH_EXPECT_EQ(a.used(), std::size_t{16});
    (void)a.allocate(8, 8);
    ZTH_EXPECT_EQ(a.used(), std::size_t{24});
}
ZTH_TEST("works", "create<T> refuses types that need a destructor (an arena never runs destructors)") {
    ZTH_EXPECT_TRUE(can_create<int>);
    ZTH_EXPECT_FALSE(can_create<std::string>);
    ZTH_EXPECT_FALSE(can_create<std::vector<int>>);
}
ZTH_TEST("works", "create<T>(args...) builds an object inside the arena") {
    CountingResource up;
    Arena a(1024, &up);
    int* i = a.create<int>(42);
    ZTH_EXPECT_TRUE(i != nullptr);
    ZTH_EXPECT_EQ(*i, 42);
    Pt* pt = a.create<Pt>(3, 4);
    ZTH_EXPECT_TRUE(pt != nullptr);
    ZTH_EXPECT_EQ(pt->x, 3);
    ZTH_EXPECT_EQ(pt->y, 4);
    ZTH_EXPECT_TRUE(aligned_to(pt, alignof(Pt)));
    double* d = a.create<double>(2.5);
    ZTH_EXPECT_TRUE(d != nullptr);
    ZTH_EXPECT_EQ(*d, 2.5);
    ZTH_EXPECT_TRUE(aligned_to(d, alignof(double)));
}
ZTH_TEST("works", "a std::pmr::vector<int> that uses the arena holds the right values") {
    CountingResource up;
    Arena a(1 << 20, &up);
    std::pmr::vector<int> v(&a);
    for (int i = 0; i < 1000; ++i) v.push_back(i * 3);
    ZTH_EXPECT_EQ(v.size(), std::size_t{1000});
    for (int i = 0; i < 1000; ++i) ZTH_EXPECT_EQ(v[static_cast<std::size_t>(i)], i * 3);
}
ZTH_TEST("works", "strings inside a vector use the arena too (the allocator is passed down to the elements)") {
    CountingResource up;
    Arena a(1 << 20, &up);
    std::pmr::vector<std::pmr::string> v(&a);
    for (int i = 0; i < 200; ++i) v.emplace_back("a string that is longer than the small-string buffer, number " + std::to_string(i));
    ZTH_EXPECT_EQ(v.size(), std::size_t{200});
    ZTH_EXPECT_EQ(std::string(v[7]), std::string("a string that is longer than the small-string buffer, number 7"));
    ZTH_EXPECT_TRUE(v[0].get_allocator().resource() == &a);
    ZTH_EXPECT_TRUE(v[199].get_allocator().resource() == &a);
}

// ================================================ Robust ======================================================
ZTH_TEST("robust", "every power-of-two alignment up to 64 is honoured, after odd sizes (the ADDRESS is aligned, not only the offset)") {
    CountingResource up;
    Arena a(1 << 16, &up);
    for (std::size_t align : {1u, 2u, 4u, 8u, 16u, 32u, 64u}) {
        (void)a.allocate(3, 1);  // make the position odd
        void* p = a.allocate(5, align);
        ZTH_EXPECT_TRUE(aligned_to(p, align));
    }
}
ZTH_TEST("robust", "a request that does not fit goes to upstream and is counted; the block is left alone") {
    CountingResource up;
    Arena a(100, &up);
    void* p1 = a.allocate(60, 1);
    ZTH_EXPECT_EQ(a.fallbacks(), std::size_t{0});
    void* p2 = a.allocate(60, 1);
    ZTH_EXPECT_EQ(a.fallbacks(), std::size_t{1});
    ZTH_EXPECT_EQ(up.allocations, std::size_t{2});  // the block, and this request
    ZTH_EXPECT_EQ(a.used(), std::size_t{60});
    ZTH_EXPECT_TRUE(p1 != p2);
    std::memset(p2, 7, 60);
    std::memset(p1, 9, 60);
    ZTH_EXPECT_EQ(static_cast<int>(static_cast<unsigned char*>(p2)[59]), 7);
}
ZTH_TEST("robust", "do_deallocate: a fallback pointer goes back to upstream, a pointer inside the block does not") {
    CountingResource up;
    {
        Arena a(100, &up);
        void* inside = a.allocate(60, 1);
        void* outside = a.allocate(60, 1);
        a.deallocate(inside, 60, 1);
        ZTH_EXPECT_EQ(up.deallocations, std::size_t{0});
        ZTH_EXPECT_EQ(up.bad_frees, std::size_t{0});
        a.deallocate(outside, 60, 1);
        ZTH_EXPECT_EQ(up.deallocations, std::size_t{1});
        ZTH_EXPECT_EQ(up.bad_frees, std::size_t{0});
    }
    ZTH_EXPECT_EQ(up.live(), std::size_t{0});
}
ZTH_TEST("robust", "a request exactly as big as the block fits; one byte more does not") {
    CountingResource up;
    Arena a(128, &up);
    (void)a.allocate(128, 1);
    ZTH_EXPECT_EQ(a.fallbacks(), std::size_t{0});
    ZTH_EXPECT_EQ(a.used(), std::size_t{128});
    void* p = a.allocate(1, 1);
    ZTH_EXPECT_TRUE(p != nullptr);
    ZTH_EXPECT_EQ(a.fallbacks(), std::size_t{1});
    a.deallocate(p, 1, 1);
}
ZTH_TEST("robust", "an absurdly large request goes to upstream (and is refused there) instead of overflowing the arithmetic") {
    CountingResource up;
    Arena a(1024, &up);
    (void)a.allocate(100, 1);  // the marker is not at 0: offset + size would wrap around for the huge sizes below
    void* p = a.allocate(~std::size_t{0} - 4, 1);
    ZTH_EXPECT_TRUE(p == nullptr);  // CountingResource refuses anything over 1 GiB
    ZTH_EXPECT_EQ(a.fallbacks(), std::size_t{1});
    ZTH_EXPECT_EQ(a.used(), std::size_t{100});
    void* q = a.allocate(~std::size_t{0} - 100, 16);
    ZTH_EXPECT_TRUE(q == nullptr);
    ZTH_EXPECT_EQ(a.fallbacks(), std::size_t{2});
}
ZTH_TEST("robust", "reset() starts again at the beginning of the same block, without asking upstream for more") {
    CountingResource up;
    Arena a(256, &up);
    void* first = a.allocate(100, 8);
    (void)a.allocate(100, 8);
    a.reset();
    ZTH_EXPECT_EQ(a.used(), std::size_t{0});
    void* again = a.allocate(100, 8);
    ZTH_EXPECT_TRUE(first == again);
    ZTH_EXPECT_EQ(a.fallbacks(), std::size_t{0});
    ZTH_EXPECT_EQ(up.allocations, std::size_t{1});
}
ZTH_TEST("robust", "zero-byte requests work, and their pointers are all different") {
    CountingResource up;
    Arena a(64, &up);
    void* p = a.allocate(0, 1);
    void* q = a.allocate(0, 1);
    ZTH_EXPECT_TRUE(p != nullptr && q != nullptr);
    ZTH_EXPECT_TRUE(p != q);
    a.deallocate(p, 0, 1);
    a.deallocate(q, 0, 1);
    ZTH_EXPECT_EQ(up.bad_frees, std::size_t{0});
}
ZTH_TEST("robust", "the destructor gives the block back to upstream: nothing is left over") {
    CountingResource up;
    {
        Arena a(4096, &up);
        (void)a.allocate(10, 1);
        (void)a.allocate(5000, 1);  // a fallback, never freed by the caller: the caller owns it
        ZTH_EXPECT_EQ(up.live(), std::size_t{2});
    }
    ZTH_EXPECT_EQ(up.live(), std::size_t{1});  // only the leaked fallback: the BLOCK went back
    ZTH_EXPECT_EQ(up.bad_frees, std::size_t{0});
    ZTH_EXPECT_EQ(up.deallocations, std::size_t{1});
}
ZTH_TEST("robust", "two arenas are different resources; an arena equals itself") {
    CountingResource up;
    Arena a(64, &up);
    Arena b(64, &up);
    ZTH_EXPECT_TRUE(a.is_equal(a));
    ZTH_EXPECT_FALSE(a.is_equal(b));
    ZTH_EXPECT_FALSE(b.is_equal(a));
}

// ================================================ Fast ========================================================
ZTH_TEST("fast", "the block is allocated once: 5000 allocations of 8 bytes are one trip to upstream, and used() is exactly 40000") {
    CountingResource up;
    Arena a(1 << 16, &up);
    for (int i = 0; i < 5000; ++i) (void)a.allocate(8, 8);
    ZTH_EXPECT_EQ(up.allocations, std::size_t{1});
    ZTH_EXPECT_EQ(a.used(), std::size_t{40000});
}
ZTH_TEST("fast", "1000 strings in a vector cost the real heap at most a few allocations (a plain vector<string> needs over a thousand)") {
    zth::mem::Scope scope;
    CountingResource up;
    {
        Arena a(1 << 20, &up);
        std::pmr::vector<std::pmr::string> v(&a);
        for (int i = 0; i < 1000; ++i) {
            char text[80];
            std::snprintf(text, sizeof text, "a name that is long enough to need heap memory %d", i);
            v.emplace_back(text);
        }
        ZTH_EXPECT_EQ(v.size(), std::size_t{1000});
        ZTH_EXPECT_EQ(a.fallbacks(), std::size_t{0});
        ZTH_EXPECT_EQ(up.allocations, std::size_t{1});
    }
    ZTH_EXPECT_ALLOCS_AT_MOST(scope, 10);
    ZTH_EXPECT_NO_MEMORY_BUGS(scope);
    ZTH_EXPECT_EQ(up.live(), std::size_t{0});
}
ZTH_TEST("fast", "reset() makes the memory reusable: 100 rounds of 1000 allocations never need a second block") {
    CountingResource up;
    Arena a(16384, &up);
    for (int round = 0; round < 100; ++round) {
        for (int i = 0; i < 1000; ++i) (void)a.allocate(8, 8);
        a.reset();
    }
    ZTH_EXPECT_EQ(a.fallbacks(), std::size_t{0});
    ZTH_EXPECT_EQ(up.allocations, std::size_t{1});
}
