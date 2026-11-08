#define ZTH_TRACK_ALLOCATIONS
#include <algorithm>
#include <cstddef>
#include <numeric>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

#include "buffer.hpp"
#include "zth_test.hpp"

namespace {

using V = std::vector<int>;

V vec(const Buffer& b) { return V(b.begin(), b.end()); }

Buffer seq(std::size_t n) {  // 0, 1, 2, ..., n-1
    Buffer b(n);
    for (std::size_t i = 0; i < n; ++i) b[i] = static_cast<int>(i);
    return b;
}

}  // namespace

// ---- Works ---------------------------------------------------------------------------------------
ZTH_TEST("works", "a default Buffer is empty") {
    Buffer b;
    ZTH_EXPECT_EQ(b.size(), std::size_t{0});
    ZTH_EXPECT_TRUE(b.empty());
    ZTH_EXPECT_TRUE(b.begin() == b.end());
}
ZTH_TEST("works", "Buffer(n, fill) holds n copies of fill (the fill defaults to 0)") {
    Buffer a(4, 7);
    ZTH_EXPECT_EQ(a.size(), std::size_t{4});
    ZTH_EXPECT_FALSE(a.empty());
    ZTH_EXPECT_EQ(vec(a), (V{7, 7, 7, 7}));
    Buffer z(3);
    ZTH_EXPECT_EQ(vec(z), (V{0, 0, 0}));
}
ZTH_TEST("works", "operator[] and at() read and write elements") {
    Buffer b(3, 0);
    b[0] = 10;
    b.at(1) = 20;
    b[2] = b.at(0) + b[1];
    ZTH_EXPECT_EQ(vec(b), (V{10, 20, 30}));
    const Buffer& c = b;
    ZTH_EXPECT_EQ(c[1], 20);
    ZTH_EXPECT_EQ(c.at(2), 30);
}
ZTH_TEST("works", "begin() and end() iterate over the elements (so std algorithms work)") {
    Buffer b = seq(5);
    ZTH_EXPECT_EQ(std::accumulate(b.begin(), b.end(), 0), 10);
    std::reverse(b.begin(), b.end());
    ZTH_EXPECT_EQ(vec(b), (V{4, 3, 2, 1, 0}));
    int n = 0;
    for (int v : b) n += v;
    ZTH_EXPECT_EQ(n, 10);
}
ZTH_TEST("works", "a copy has the same elements and a block of its own") {
    Buffer a = seq(4);
    Buffer b(a);
    ZTH_EXPECT_EQ(vec(b), (V{0, 1, 2, 3}));
    ZTH_EXPECT_TRUE(a.begin() != b.begin());
    b[0] = 99;
    ZTH_EXPECT_EQ(vec(a), (V{0, 1, 2, 3}));
    Buffer c;
    c = a;
    c[3] = -1;
    ZTH_EXPECT_EQ(vec(c), (V{0, 1, 2, -1}));
    ZTH_EXPECT_EQ(vec(a), (V{0, 1, 2, 3}));
}
ZTH_TEST("works", "a move takes the block and leaves the source empty") {
    Buffer a = seq(4);
    const int* block = a.begin();
    Buffer b(std::move(a));
    ZTH_EXPECT_TRUE(b.begin() == block);
    ZTH_EXPECT_EQ(vec(b), (V{0, 1, 2, 3}));
    ZTH_EXPECT_TRUE(a.empty());
    ZTH_EXPECT_EQ(a.size(), std::size_t{0});
    Buffer c = seq(2);
    c = std::move(b);
    ZTH_EXPECT_TRUE(c.begin() == block);
    ZTH_EXPECT_TRUE(b.empty());
}
ZTH_TEST("works", "swap exchanges two Buffers") {
    Buffer a = seq(3), b(5, 9);
    a.swap(b);
    ZTH_EXPECT_EQ(vec(a), (V{9, 9, 9, 9, 9}));
    ZTH_EXPECT_EQ(vec(b), (V{0, 1, 2}));
    swap(a, b);
    ZTH_EXPECT_EQ(vec(a), (V{0, 1, 2}));
}
ZTH_TEST("works", "resize keeps the first values and fills new ones with 0") {
    Buffer b = seq(3);
    b.resize(5);
    ZTH_EXPECT_EQ(vec(b), (V{0, 1, 2, 0, 0}));
    b.resize(2);
    ZTH_EXPECT_EQ(vec(b), (V{0, 1}));
    b.resize(2);
    ZTH_EXPECT_EQ(vec(b), (V{0, 1}));
    ZTH_EXPECT_EQ(b.size(), std::size_t{2});
}
ZTH_TEST("works", "== compares sizes and every element") {
    ZTH_EXPECT_TRUE(seq(3) == seq(3));
    ZTH_EXPECT_FALSE(seq(3) == seq(4));
    Buffer a = seq(3), b = seq(3);
    b[2] = 7;
    ZTH_EXPECT_FALSE(a == b);
    ZTH_EXPECT_TRUE(Buffer() == Buffer());
}
ZTH_TEST("works", "building, copying, moving, resizing and destroying Buffers leaks nothing and frees nothing twice") {
    zth::mem::Scope scope;
    {
        Buffer a = seq(10);
        Buffer b = a;
        Buffer c = std::move(a);
        a = b;
        c.resize(20);
        b.swap(c);
        Buffer d;
        d = std::move(b);
    }
    ZTH_EXPECT_NO_MEMORY_BUGS(scope);
}

// ---- Robust --------------------------------------------------------------------------------------
ZTH_TEST("robust", "at() throws std::out_of_range for an index >= size (const and non-const)") {
    Buffer b = seq(3);
    const Buffer& c = b;
    ZTH_EXPECT_THROWS(std::out_of_range, (void)b.at(3));
    ZTH_EXPECT_THROWS(std::out_of_range, (void)c.at(3));
    ZTH_EXPECT_THROWS(std::out_of_range, (void)c.at(1000));
    ZTH_EXPECT_NO_THROW((void)b.at(2));
    Buffer e;
    ZTH_EXPECT_THROWS(std::out_of_range, (void)e.at(0));
}
ZTH_TEST("robust", "assigning a Buffer to itself, by copy or by move, leaves it intact") {
    Buffer a = seq(4);
    Buffer& same = a;  // (the compiler warns about a = a; this is the same thing through a reference)
    a = same;
    ZTH_EXPECT_EQ(vec(a), (V{0, 1, 2, 3}));
    a = std::move(same);
    ZTH_EXPECT_EQ(vec(a), (V{0, 1, 2, 3}));
    a.resize(a.size());
    ZTH_EXPECT_EQ(vec(a), (V{0, 1, 2, 3}));
    a.swap(same);
    ZTH_EXPECT_EQ(vec(a), (V{0, 1, 2, 3}));
}
ZTH_TEST("robust", "empty Buffers copy, move, swap and resize correctly") {
    Buffer e;
    Buffer copy = e;
    ZTH_EXPECT_TRUE(copy.empty());
    Buffer moved = std::move(e);
    ZTH_EXPECT_TRUE(moved.empty());
    Buffer zero(0, 5);
    ZTH_EXPECT_TRUE(zero.empty());
    Buffer a = seq(3);
    a.resize(0);
    ZTH_EXPECT_TRUE(a.empty());
    a.resize(2);
    ZTH_EXPECT_EQ(vec(a), (V{0, 0}));
    Buffer b;
    b.swap(a);
    ZTH_EXPECT_EQ(vec(b), (V{0, 0}));
    ZTH_EXPECT_TRUE(a.empty());
    Buffer x = seq(3);
    x = e;  // assigning an empty one
    ZTH_EXPECT_TRUE(x.empty());
}
ZTH_TEST("robust", "a moved-from Buffer can be resized, assigned to and destroyed") {
    zth::mem::Scope scope;
    {
        Buffer a = seq(5);
        Buffer b = std::move(a);
        a.resize(3);
        ZTH_EXPECT_EQ(vec(a), (V{0, 0, 0}));
        Buffer c = std::move(a);
        a = seq(2);
        ZTH_EXPECT_EQ(vec(a), (V{0, 1}));
        ZTH_EXPECT_EQ(vec(b), (V{0, 1, 2, 3, 4}));
    }
    ZTH_EXPECT_NO_MEMORY_BUGS(scope);
}
ZTH_TEST("robust", "resize to a smaller size does not read or write past the new end; growing does not read garbage") {
    zth::mem::Scope scope;
    {
        Buffer big = seq(100);
        big.resize(10);
        ZTH_EXPECT_EQ(vec(big), (V{0, 1, 2, 3, 4, 5, 6, 7, 8, 9}));
        big.resize(1000);
        ZTH_EXPECT_EQ(big.size(), std::size_t{1000});
        for (std::size_t i = 10; i < 1000; ++i) ZTH_EXPECT_EQ(big[i], 0);
        big.resize(0);
        big.resize(3);
        ZTH_EXPECT_EQ(vec(big), (V{0, 0, 0}));
    }
    ZTH_EXPECT_NO_MEMORY_BUGS(scope);
}
ZTH_TEST("robust", "the move operations and swap are noexcept") {
    ZTH_EXPECT_TRUE((std::is_nothrow_move_constructible_v<Buffer>));
    ZTH_EXPECT_TRUE((std::is_nothrow_move_assignable_v<Buffer>));
    ZTH_EXPECT_TRUE((std::is_nothrow_swappable_v<Buffer>));
    ZTH_EXPECT_TRUE((std::is_nothrow_destructible_v<Buffer>));
}
ZTH_TEST("robust", "a vector of Buffers keeps its values while it grows, and sorts, erases and inserts without leaks") {
    zth::mem::Scope scope;
    {
        std::vector<Buffer> v;
        for (int i = 0; i < 40; ++i) v.push_back(Buffer(static_cast<std::size_t>(i % 7) + 1, i));
        ZTH_EXPECT_EQ(v[33].size(), std::size_t{6});
        ZTH_EXPECT_EQ(v[33][5], 33);
        std::sort(v.begin(), v.end(), [](const Buffer& x, const Buffer& y) { return x.size() < y.size() || (x.size() == y.size() && x[0] < y[0]); });
        ZTH_EXPECT_EQ(v.front().size(), std::size_t{1});
        ZTH_EXPECT_EQ(v.back().size(), std::size_t{7});
        v.erase(v.begin() + 5, v.begin() + 15);
        v.insert(v.begin() + 2, seq(3));
        ZTH_EXPECT_EQ(v.size(), std::size_t{31});
        ZTH_EXPECT_EQ(vec(v[2]), (V{0, 1, 2}));
        std::vector<Buffer> copy = v;
        copy[0][0] = -5;
        ZTH_EXPECT_TRUE(v[0][0] != -5);
    }
    ZTH_EXPECT_NO_MEMORY_BUGS(scope);
}
ZTH_TEST("robust", "large Buffers copy and move intact") {
    Buffer a(100000, 3);
    a[99999] = 42;
    Buffer b = a;
    Buffer c = std::move(a);
    ZTH_EXPECT_EQ(b.size(), std::size_t{100000});
    ZTH_EXPECT_EQ(b[99999], 42);
    ZTH_EXPECT_EQ(c[99999], 42);
    ZTH_EXPECT_TRUE(b == c);
}
ZTH_TEST("robust", "copy assignment to a Buffer of another size releases the old block and takes the new size") {
    zth::mem::Scope scope;
    {
        Buffer a = seq(3), b = seq(50);
        for (int i = 0; i < 20; ++i) {
            a = b;
            ZTH_EXPECT_EQ(a.size(), std::size_t{50});
            b = Buffer(2, i);
            ZTH_EXPECT_EQ(b.size(), std::size_t{2});
            a = b;
            ZTH_EXPECT_EQ(a.size(), std::size_t{2});
            b = seq(50);
        }
    }
    ZTH_EXPECT_NO_MEMORY_BUGS(scope);
}

// ---- Fast: copies allocate once, moves allocate nothing -------------------------------------------------------
ZTH_TEST("fast", "default construction and Buffer(0) allocate nothing") {
    zth::mem::Scope scope;
    Buffer a;
    Buffer b(0);
    Buffer c(0, 5);
    ZTH_EXPECT_ALLOCS_AT_MOST(scope, 0);
}
ZTH_TEST("fast", "a copy allocates exactly one block; copying an empty Buffer allocates none") {
    Buffer a = seq(100);
    Buffer e;
    zth::mem::Scope scope;
    Buffer b(a);
    ZTH_EXPECT_ALLOCS_AT_MOST(scope, 1);
    Buffer c(e);
    ZTH_EXPECT_ALLOCS_AT_MOST(scope, 1);
}
ZTH_TEST("fast", "moving and swapping allocate nothing") {
    Buffer a = seq(100), b = seq(50), c = seq(10);
    zth::mem::Scope scope;
    Buffer d(std::move(a));
    b = std::move(c);
    d.swap(b);
    swap(d, b);
    std::swap(d, b);
    ZTH_EXPECT_ALLOCS_AT_MOST(scope, 0);
}
ZTH_TEST("fast", "resize allocates at most one block, and nothing when the size does not change") {
    Buffer a = seq(10);
    zth::mem::Scope scope;
    a.resize(10);
    ZTH_EXPECT_ALLOCS_AT_MOST(scope, 0);
    a.resize(20);
    ZTH_EXPECT_ALLOCS_AT_MOST(scope, 1);
    a.resize(0);
    ZTH_EXPECT_ALLOCS_AT_MOST(scope, 1);
}
ZTH_TEST("fast", "a vector of Buffers moves them when it grows: 100 push_backs allocate about 100 blocks, not 300") {
    std::vector<Buffer> v;
    zth::mem::Scope scope;
    for (int i = 0; i < 100; ++i) v.push_back(Buffer(8, i));
    ZTH_EXPECT_ALLOCS_AT_MOST(scope, 115);  // 100 blocks + 8 reallocations of the vector's own array
}

// ---- Clean ---------------------------------------------------------------------------------------
ZTH_TEST("clean", "buffer.hpp does not use malloc, calloc, realloc or free (new[] and delete[] only)") {
    const std::string code = ::zth::strip_comments_and_literals(::zth::read_file("buffer.hpp"));
    for (const char* word : {"malloc", "calloc", "realloc", "free"}) {
        if (!::zth::find_word(code, word).empty()) {
            ::zth::fail(std::string("buffer.hpp uses ") + word + ": use new[] / delete[] (or better, a std container) instead");
            return;
        }
    }
    ZTH_EXPECT_TRUE(true);
}
