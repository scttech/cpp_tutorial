#define ZTH_TRACK_ALLOCATIONS
#include <algorithm>
#include <cstddef>
#include <numeric>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

#include "int_vector.hpp"
#include "zth_test.hpp"

namespace {

using V = std::vector<int>;

V vec(const IntVector& v) { return V(v.begin(), v.end()); }

IntVector seq(int n) {  // 0, 1, ..., n-1, built with push_back
    IntVector v;
    for (int i = 0; i < n; ++i) v.push_back(i);
    return v;
}

}  // namespace

// ================================================ Works =======================================================
ZTH_TEST("works", "a default IntVector is empty, with no capacity") {
    IntVector v;
    ZTH_EXPECT_EQ(v.size(), std::size_t{0});
    ZTH_EXPECT_EQ(v.capacity(), std::size_t{0});
    ZTH_EXPECT_TRUE(v.empty());
    ZTH_EXPECT_TRUE(v.begin() == v.end());
}
ZTH_TEST("works", "IntVector(count, value) holds count copies; IntVector{a, b, c} holds those values (braces pick the list)") {
    IntVector a(3, 7);
    ZTH_EXPECT_EQ(vec(a), (V{7, 7, 7}));
    ZTH_EXPECT_EQ(a.capacity(), std::size_t{3});
    IntVector z(2);
    ZTH_EXPECT_EQ(vec(z), (V{0, 0}));
    IntVector b{3, 7};
    ZTH_EXPECT_EQ(vec(b), (V{3, 7}));
    IntVector c{1, 2, 3, 4};
    ZTH_EXPECT_EQ(vec(c), (V{1, 2, 3, 4}));
    ZTH_EXPECT_EQ(c.capacity(), std::size_t{4});
}
ZTH_TEST("works", "push_back appends, and the capacity doubles when full: 1, 2, 4, 8, 16") {
    IntVector v;
    const std::size_t expected[] = {1, 2, 4, 4, 8, 8, 8, 8, 16, 16};
    for (int i = 0; i < 10; ++i) {
        v.push_back(i * 10);
        ZTH_EXPECT_EQ(v.size(), static_cast<std::size_t>(i) + 1);
        ZTH_EXPECT_EQ(v.capacity(), expected[i]);
    }
    ZTH_EXPECT_EQ(vec(v), (V{0, 10, 20, 30, 40, 50, 60, 70, 80, 90}));
}
ZTH_TEST("works", "operator[], at(), front() and back() read and write") {
    IntVector v{10, 20, 30};
    v[0] = 11;
    v.at(1) = 22;
    v.back() = 33;
    ZTH_EXPECT_EQ(vec(v), (V{11, 22, 33}));
    ZTH_EXPECT_EQ(v.front(), 11);
    ZTH_EXPECT_EQ(v.back(), 33);
    const IntVector& c = v;
    ZTH_EXPECT_EQ(c[2], 33);
    ZTH_EXPECT_EQ(c.at(0), 11);
    ZTH_EXPECT_EQ(c.front(), 11);
    ZTH_EXPECT_EQ(c.back(), 33);
    ZTH_EXPECT_TRUE(v.data() == v.begin());
}
ZTH_TEST("works", "pop_back removes the last element and keeps the capacity") {
    IntVector v{1, 2, 3};
    v.pop_back();
    ZTH_EXPECT_EQ(vec(v), (V{1, 2}));
    ZTH_EXPECT_EQ(v.capacity(), std::size_t{3});
    v.pop_back();
    v.pop_back();
    ZTH_EXPECT_TRUE(v.empty());
}
ZTH_TEST("works", "insert puts a value before a position: at the front, in the middle, at the end") {
    IntVector v{2, 4};
    v.insert(0, 1);
    ZTH_EXPECT_EQ(vec(v), (V{1, 2, 4}));
    v.insert(2, 3);
    ZTH_EXPECT_EQ(vec(v), (V{1, 2, 3, 4}));
    v.insert(4, 5);
    ZTH_EXPECT_EQ(vec(v), (V{1, 2, 3, 4, 5}));
    IntVector e;
    e.insert(0, 9);
    ZTH_EXPECT_EQ(vec(e), (V{9}));
}
ZTH_TEST("works", "erase removes the element at a position: the front, the middle, the last") {
    IntVector v{1, 2, 3, 4, 5};
    v.erase(0);
    ZTH_EXPECT_EQ(vec(v), (V{2, 3, 4, 5}));
    v.erase(1);
    ZTH_EXPECT_EQ(vec(v), (V{2, 4, 5}));
    v.erase(2);
    ZTH_EXPECT_EQ(vec(v), (V{2, 4}));
    ZTH_EXPECT_EQ(v.capacity(), std::size_t{5});
}
ZTH_TEST("works", "clear empties the vector but keeps its capacity") {
    IntVector v = seq(10);
    const std::size_t cap = v.capacity();
    v.clear();
    ZTH_EXPECT_TRUE(v.empty());
    ZTH_EXPECT_EQ(v.capacity(), cap);
    v.push_back(1);
    ZTH_EXPECT_EQ(vec(v), (V{1}));
}
ZTH_TEST("works", "reserve sets the capacity to exactly n when it has to grow, never shrinks, and keeps the elements") {
    IntVector v{1, 2, 3};
    v.reserve(20);
    ZTH_EXPECT_EQ(v.capacity(), std::size_t{20});
    ZTH_EXPECT_EQ(vec(v), (V{1, 2, 3}));
    v.reserve(5);
    ZTH_EXPECT_EQ(v.capacity(), std::size_t{20});
    v.reserve(0);
    ZTH_EXPECT_EQ(v.capacity(), std::size_t{20});
}
ZTH_TEST("works", "shrink_to_fit makes the capacity equal to the size") {
    IntVector v = seq(5);
    ZTH_EXPECT_EQ(v.capacity(), std::size_t{8});
    v.shrink_to_fit();
    ZTH_EXPECT_EQ(v.capacity(), std::size_t{5});
    ZTH_EXPECT_EQ(vec(v), (V{0, 1, 2, 3, 4}));
    v.clear();
    v.shrink_to_fit();
    ZTH_EXPECT_EQ(v.capacity(), std::size_t{0});
}
ZTH_TEST("works", "resize shrinks, or grows with copies of the value (0 by default)") {
    IntVector v{1, 2, 3};
    v.resize(5);
    ZTH_EXPECT_EQ(vec(v), (V{1, 2, 3, 0, 0}));
    v.resize(7, 9);
    ZTH_EXPECT_EQ(vec(v), (V{1, 2, 3, 0, 0, 9, 9}));
    v.resize(2);
    ZTH_EXPECT_EQ(vec(v), (V{1, 2}));
    ZTH_EXPECT_TRUE(v.capacity() >= 7);
}
ZTH_TEST("works", "a copy is deep and has capacity == size") {
    IntVector a = seq(5);
    IntVector b = a;
    ZTH_EXPECT_EQ(vec(b), vec(a));
    ZTH_EXPECT_EQ(b.capacity(), std::size_t{5});
    ZTH_EXPECT_TRUE(a.data() != b.data());
    b[0] = 99;
    b.push_back(7);
    ZTH_EXPECT_EQ(vec(a), (V{0, 1, 2, 3, 4}));
    IntVector c;
    c = a;
    c.erase(0);
    ZTH_EXPECT_EQ(vec(a), (V{0, 1, 2, 3, 4}));
    ZTH_EXPECT_EQ(vec(c), (V{1, 2, 3, 4}));
}
ZTH_TEST("works", "a move takes the block and leaves the source empty (size 0, capacity 0)") {
    IntVector a = seq(5);
    const int* block = a.data();
    IntVector b(std::move(a));
    ZTH_EXPECT_TRUE(b.data() == block);
    ZTH_EXPECT_EQ(vec(b), (V{0, 1, 2, 3, 4}));
    ZTH_EXPECT_EQ(b.capacity(), std::size_t{8});
    ZTH_EXPECT_EQ(a.size(), std::size_t{0});
    ZTH_EXPECT_EQ(a.capacity(), std::size_t{0});
    IntVector c{9};
    c = std::move(b);
    ZTH_EXPECT_TRUE(c.data() == block);
    ZTH_EXPECT_TRUE(b.empty());
    ZTH_EXPECT_EQ(b.capacity(), std::size_t{0});
}
ZTH_TEST("works", "swap, == and the standard algorithms work") {
    IntVector a{3, 1, 2}, b{9};
    a.swap(b);
    ZTH_EXPECT_EQ(vec(a), (V{9}));
    ZTH_EXPECT_EQ(vec(b), (V{3, 1, 2}));
    swap(a, b);
    ZTH_EXPECT_EQ(vec(a), (V{3, 1, 2}));
    std::sort(a.begin(), a.end());
    ZTH_EXPECT_EQ(vec(a), (V{1, 2, 3}));
    ZTH_EXPECT_EQ(std::accumulate(a.begin(), a.end(), 0), 6);
    ZTH_EXPECT_TRUE(a == (IntVector{1, 2, 3}));
    ZTH_EXPECT_FALSE(a == (IntVector{1, 2}));
    ZTH_EXPECT_FALSE(a == (IntVector{1, 2, 4}));
    ZTH_EXPECT_TRUE(IntVector() == IntVector());
}
ZTH_TEST("works", "everything above leaks nothing and frees nothing twice") {
    zth::mem::Scope scope;
    {
        IntVector a = seq(20);
        IntVector b = a;
        IntVector c = std::move(a);
        a = b;
        c.insert(3, 99);
        c.erase(0);
        c.reserve(100);
        c.shrink_to_fit();
        b.resize(50, 1);
        b.swap(c);
        IntVector d;
        d = std::move(b);
        d.clear();
    }
    ZTH_EXPECT_NO_MEMORY_BUGS(scope);
}

// ================================================ Robust ======================================================
ZTH_TEST("robust", "at(), front(), back(), pop_back() on a bad index or an empty vector throw std::out_of_range") {
    IntVector v{1, 2, 3};
    const IntVector& c = v;
    ZTH_EXPECT_THROWS(std::out_of_range, (void)v.at(3));
    ZTH_EXPECT_THROWS(std::out_of_range, (void)c.at(3));
    ZTH_EXPECT_NO_THROW((void)v.at(2));
    IntVector e;
    ZTH_EXPECT_THROWS(std::out_of_range, (void)e.front());
    ZTH_EXPECT_THROWS(std::out_of_range, (void)e.back());
    ZTH_EXPECT_THROWS(std::out_of_range, e.pop_back());
    const IntVector& ce = e;
    ZTH_EXPECT_THROWS(std::out_of_range, (void)ce.front());
    ZTH_EXPECT_THROWS(std::out_of_range, (void)ce.back());
    ZTH_EXPECT_TRUE(e.empty());
}
ZTH_TEST("robust", "insert and erase check their position (std::out_of_range) and change nothing when they throw") {
    IntVector v{1, 2, 3};
    ZTH_EXPECT_THROWS(std::out_of_range, v.insert(4, 9));
    ZTH_EXPECT_THROWS(std::out_of_range, v.erase(3));
    ZTH_EXPECT_THROWS(std::out_of_range, v.erase(100));
    ZTH_EXPECT_EQ(vec(v), (V{1, 2, 3}));
    ZTH_EXPECT_NO_THROW(v.insert(3, 4));  // == size() is allowed
    IntVector e;
    ZTH_EXPECT_THROWS(std::out_of_range, e.erase(0));
    ZTH_EXPECT_THROWS(std::out_of_range, e.insert(1, 1));
}
ZTH_TEST("robust", "push_back of one of the vector's own elements works even when it has to reallocate") {
    IntVector v{5, 6, 7, 8};  // full: capacity 4
    v.push_back(v[0]);
    ZTH_EXPECT_EQ(vec(v), (V{5, 6, 7, 8, 5}));
    IntVector w{1};
    w.push_back(w[0]);
    w.push_back(w.back());
    w.push_back(w.front());
    ZTH_EXPECT_EQ(vec(w), (V{1, 1, 1, 1}));
    IntVector u = seq(8);  // capacity 8, full
    u.push_back(u[7]);
    u.push_back(u.at(3));
    ZTH_EXPECT_EQ(u.size(), std::size_t{10});
    ZTH_EXPECT_EQ(u[8], 7);
    ZTH_EXPECT_EQ(u[9], 3);
}
ZTH_TEST("robust", "insert of one of the vector's own elements works, with and without room, before and after it") {
    IntVector full{1, 2, 3, 4};  // capacity 4: insert must reallocate
    full.insert(0, full[3]);
    ZTH_EXPECT_EQ(vec(full), (V{4, 1, 2, 3, 4}));
    IntVector roomy = seq(5);
    roomy.reserve(20);
    roomy.insert(0, roomy[4]);  // elements shift up by one: the source element moves while it is being copied
    ZTH_EXPECT_EQ(vec(roomy), (V{4, 0, 1, 2, 3, 4}));
    roomy.insert(2, roomy[1]);
    ZTH_EXPECT_EQ(vec(roomy), (V{4, 0, 0, 1, 2, 3, 4}));
    IntVector shifting{10, 20, 30};
    shifting.reserve(10);
    shifting.insert(0, shifting[0]);
    ZTH_EXPECT_EQ(vec(shifting), (V{10, 10, 20, 30}));
    shifting.insert(1, shifting[3]);
    ZTH_EXPECT_EQ(vec(shifting), (V{10, 30, 10, 20, 30}));
}
ZTH_TEST("robust", "resize with one of the vector's own elements as the value works when it reallocates") {
    IntVector v{7, 8};
    v.resize(6, v[1]);
    ZTH_EXPECT_EQ(vec(v), (V{7, 8, 8, 8, 8, 8}));
}
ZTH_TEST("robust", "reserve then push_back never reallocates until the reserved capacity is used up (the block stays put)") {
    IntVector v;
    v.reserve(50);
    const int* block = v.data();
    for (int i = 0; i < 50; ++i) v.push_back(i);
    ZTH_EXPECT_TRUE(v.data() == block);
    ZTH_EXPECT_EQ(v.capacity(), std::size_t{50});
    v.push_back(50);
    ZTH_EXPECT_EQ(v.capacity(), std::size_t{100});
    ZTH_EXPECT_EQ(v[50], 50);
    ZTH_EXPECT_EQ(v[0], 0);
}
ZTH_TEST("robust", "insert and erase inside the capacity keep the block and the capacity") {
    IntVector v;
    v.reserve(20);
    for (int i = 0; i < 10; ++i) v.push_back(i);
    const int* block = v.data();
    v.insert(5, 100);
    v.erase(0);
    ZTH_EXPECT_TRUE(v.data() == block);
    ZTH_EXPECT_EQ(v.capacity(), std::size_t{20});
    ZTH_EXPECT_EQ(v.size(), std::size_t{10});
}
ZTH_TEST("robust", "assigning to itself, by copy or by move, leaves the vector intact") {
    IntVector a = seq(6);
    IntVector& same = a;  // (a = a is flagged by the compiler; this is the same thing through a reference)
    a = same;
    ZTH_EXPECT_EQ(vec(a), (V{0, 1, 2, 3, 4, 5}));
    a = std::move(same);
    ZTH_EXPECT_EQ(vec(a), (V{0, 1, 2, 3, 4, 5}));
    a.swap(same);
    ZTH_EXPECT_EQ(vec(a), (V{0, 1, 2, 3, 4, 5}));
}
ZTH_TEST("robust", "empty vectors copy, move, swap, resize, reserve and shrink correctly") {
    IntVector e;
    IntVector copy = e;
    ZTH_EXPECT_TRUE(copy.empty());
    ZTH_EXPECT_EQ(copy.capacity(), std::size_t{0});
    IntVector moved = std::move(e);
    ZTH_EXPECT_TRUE(moved.empty());
    IntVector zero(0, 5);
    ZTH_EXPECT_TRUE(zero.empty());
    IntVector list{};
    ZTH_EXPECT_TRUE(list.empty());
    moved.shrink_to_fit();
    moved.resize(0);
    moved.reserve(0);
    moved.clear();
    ZTH_EXPECT_EQ(moved.capacity(), std::size_t{0});
    IntVector x = seq(3);
    x = e;
    ZTH_EXPECT_TRUE(x.empty());
}
ZTH_TEST("robust", "a moved-from vector can be used again") {
    zth::mem::Scope scope;
    {
        IntVector a = seq(5);
        IntVector b = std::move(a);
        a.push_back(1);
        a.push_back(2);
        ZTH_EXPECT_EQ(vec(a), (V{1, 2}));
        ZTH_EXPECT_EQ(vec(b), (V{0, 1, 2, 3, 4}));
        IntVector c = std::move(a);
        a = seq(3);
        a.insert(0, 7);
        ZTH_EXPECT_EQ(vec(a), (V{7, 0, 1, 2}));
    }
    ZTH_EXPECT_NO_MEMORY_BUGS(scope);
}
ZTH_TEST("robust", "the move operations and swap are noexcept, and the vector is nothrow-destructible") {
    ZTH_EXPECT_TRUE((std::is_nothrow_move_constructible_v<IntVector>));
    ZTH_EXPECT_TRUE((std::is_nothrow_move_assignable_v<IntVector>));
    ZTH_EXPECT_TRUE((std::is_nothrow_swappable_v<IntVector>));
    ZTH_EXPECT_TRUE((std::is_nothrow_destructible_v<IntVector>));
    ZTH_EXPECT_FALSE((std::is_convertible_v<std::size_t, IntVector>));  // the count constructor is explicit
}
ZTH_TEST("robust", "copy and move assignment over a vector that already has a block release the old block") {
    zth::mem::Scope scope;
    {
        IntVector a = seq(10), b = seq(30), c = seq(5);
        for (int i = 0; i < 20; ++i) {
            a = b;
            b = c;
            c = seq(7);
            a = std::move(c);
            c = a;
        }
        ZTH_EXPECT_EQ(a.size(), std::size_t{7});
    }
    ZTH_EXPECT_NO_MEMORY_BUGS(scope);
}
ZTH_TEST("robust", "swap exchanges the capacities too, and the member swap is noexcept") {
    IntVector a = seq(3), b = seq(9);
    ZTH_EXPECT_EQ(a.capacity(), std::size_t{4});
    ZTH_EXPECT_EQ(b.capacity(), std::size_t{16});
    a.swap(b);
    ZTH_EXPECT_EQ(a.capacity(), std::size_t{16});
    ZTH_EXPECT_EQ(b.capacity(), std::size_t{4});
    ZTH_EXPECT_EQ(a.size(), std::size_t{9});
    ZTH_EXPECT_EQ(b.size(), std::size_t{3});
    swap(a, b);
    ZTH_EXPECT_EQ(a.capacity(), std::size_t{4});
    ZTH_EXPECT_TRUE(noexcept(std::declval<IntVector&>().swap(std::declval<IntVector&>())));
}
ZTH_TEST("robust", "a std::vector of IntVectors keeps its values while it grows, sorts, erases and inserts, without leaks") {
    zth::mem::Scope scope;
    {
        std::vector<IntVector> v;
        for (int i = 0; i < 40; ++i) v.push_back(IntVector(static_cast<std::size_t>(i % 5) + 1, i));
        ZTH_EXPECT_EQ(v[33].size(), std::size_t{4});
        ZTH_EXPECT_EQ(v[33][3], 33);
        std::sort(v.begin(), v.end(), [](const IntVector& x, const IntVector& y) { return x.size() < y.size() || (x.size() == y.size() && x[0] < y[0]); });
        ZTH_EXPECT_EQ(v.front().size(), std::size_t{1});
        v.erase(v.begin() + 3, v.begin() + 13);
        v.insert(v.begin() + 1, IntVector{1, 2, 3});
        ZTH_EXPECT_EQ(v.size(), std::size_t{31});
        ZTH_EXPECT_EQ(vec(v[1]), (V{1, 2, 3}));
        std::vector<IntVector> copy = v;
        copy[0].push_back(-1);
        ZTH_EXPECT_TRUE(v[0].size() == 1);
    }
    ZTH_EXPECT_NO_MEMORY_BUGS(scope);
}
ZTH_TEST("robust", "a long random sequence of operations always agrees with std::vector (and size <= capacity)") {
    IntVector mine;
    V model;
    zth::Lcg rng(777);
    auto next = [&](int mod) { return rng.below(mod); };
    for (int step = 0; step < 3000; ++step) {
        const int op = next(14);
        const int value = next(1000) - 500;
        switch (op) {
            case 0: case 1: case 2: case 3:
                mine.push_back(value);
                model.push_back(value);
                break;
            case 4:
                if (!model.empty()) {
                    mine.pop_back();
                    model.pop_back();
                }
                break;
            case 5: case 6: {
                const std::size_t pos = static_cast<std::size_t>(next(static_cast<int>(model.size()) + 1));
                mine.insert(pos, value);
                model.insert(model.begin() + static_cast<std::ptrdiff_t>(pos), value);
                break;
            }
            case 7: case 8:
                if (!model.empty()) {
                    const std::size_t pos = static_cast<std::size_t>(next(static_cast<int>(model.size())));
                    mine.erase(pos);
                    model.erase(model.begin() + static_cast<std::ptrdiff_t>(pos));
                }
                break;
            case 9: {
                const std::size_t n = static_cast<std::size_t>(next(40));
                mine.resize(n, value);
                model.resize(n, value);
                break;
            }
            case 10:
                mine.reserve(static_cast<std::size_t>(next(60)));
                break;
            case 11:
                if (next(4) == 0) {
                    mine.shrink_to_fit();
                    ZTH_EXPECT_EQ(mine.capacity(), mine.size());
                }
                break;
            case 12:
                if (next(10) == 0) {
                    mine.clear();
                    model.clear();
                }
                break;
            default:
                if (!model.empty()) {  // push_back of one of the vector's own elements
                    const std::size_t k = static_cast<std::size_t>(next(static_cast<int>(model.size())));
                    mine.push_back(mine[k]);
                    model.push_back(model[k]);
                }
                break;
        }
        if (mine.size() != model.size() || mine.capacity() < mine.size() || !std::equal(model.begin(), model.end(), mine.begin())) {
            ::zth::fail("after step " + std::to_string(step) + " (operation " + std::to_string(op) + "), the IntVector is " + std::to_string(mine.size()) +
                        " elements (capacity " + std::to_string(mine.capacity()) + ") but it should be " + std::to_string(model.size()) + " elements, or its elements differ");
            return;
        }
    }
}
ZTH_TEST("robust", "a hundred thousand push_backs") {
    IntVector v;
    for (int i = 0; i < 100000; ++i) v.push_back(i);
    ZTH_EXPECT_EQ(v.size(), std::size_t{100000});
    ZTH_EXPECT_EQ(v.back(), 99999);
    ZTH_EXPECT_EQ(v.capacity(), std::size_t{131072});
    long long sum = 0;
    for (int x : v) sum += x;
    ZTH_EXPECT_EQ(sum, 4999950000LL);
}

// ================================================ Fast ========================================================
ZTH_TEST("fast", "n push_backs allocate about log2(n) blocks, not n (the capacity grows geometrically)") {
    IntVector v;
    zth::mem::Scope scope;
    for (int i = 0; i < 1000; ++i) v.push_back(i);
    ZTH_EXPECT_ALLOCS_AT_MOST(scope, 11);  // capacities 1, 2, 4, ..., 1024
}
ZTH_TEST("fast", "reserve first, and a thousand push_backs allocate nothing more") {
    IntVector v;
    v.reserve(1000);
    zth::mem::Scope scope;
    for (int i = 0; i < 1000; ++i) v.push_back(i);
    ZTH_EXPECT_ALLOCS_AT_MOST(scope, 0);
}
ZTH_TEST("fast", "constructors and a copy allocate exactly one block (none for an empty vector)") {
    zth::mem::Scope scope;
    IntVector e;
    IntVector zero(0);
    IntVector emptylist{};
    ZTH_EXPECT_ALLOCS_AT_MOST(scope, 0);
    IntVector a(100, 1);
    ZTH_EXPECT_ALLOCS_AT_MOST(scope, 1);
    IntVector b{1, 2, 3, 4, 5};
    ZTH_EXPECT_ALLOCS_AT_MOST(scope, 2);
    IntVector c(a);
    ZTH_EXPECT_ALLOCS_AT_MOST(scope, 3);
    IntVector d(e);
    ZTH_EXPECT_ALLOCS_AT_MOST(scope, 3);
}
ZTH_TEST("fast", "move, swap, pop_back, clear and erase allocate nothing") {
    IntVector a = seq(100), b = seq(50), c = seq(10);
    zth::mem::Scope scope;
    IntVector d(std::move(a));
    b = std::move(c);
    d.swap(b);
    swap(d, b);
    std::swap(d, b);
    d.pop_back();
    d.erase(3);
    b.clear();
    ZTH_EXPECT_ALLOCS_AT_MOST(scope, 0);
}
ZTH_TEST("fast", "insert and resize inside the capacity allocate nothing; reserve and shrink_to_fit allocate one block at most") {
    IntVector v;
    v.reserve(100);
    for (int i = 0; i < 10; ++i) v.push_back(i);
    zth::mem::Scope scope;
    v.insert(0, 1);
    v.insert(5, 2);
    v.resize(50, 3);
    v.resize(20);
    ZTH_EXPECT_ALLOCS_AT_MOST(scope, 0);
    v.shrink_to_fit();
    ZTH_EXPECT_ALLOCS_AT_MOST(scope, 1);
    v.shrink_to_fit();
    ZTH_EXPECT_ALLOCS_AT_MOST(scope, 1);
    v.reserve(500);
    ZTH_EXPECT_ALLOCS_AT_MOST(scope, 2);
    v.reserve(400);
    ZTH_EXPECT_ALLOCS_AT_MOST(scope, 2);
}
ZTH_TEST("fast", "a std::vector of IntVectors moves them when it grows: 100 push_backs allocate about 100 blocks, not 300") {
    std::vector<IntVector> v;
    zth::mem::Scope scope;
    for (int i = 0; i < 100; ++i) v.push_back(IntVector(8, i));
    ZTH_EXPECT_ALLOCS_AT_MOST(scope, 115);
}

// ================================================ Clean =======================================================
ZTH_TEST("clean", "int_vector.hpp does not use malloc, calloc, realloc or free (new[] and delete[] only)") {
    const std::string code = ::zth::strip_comments_and_literals(::zth::read_file("int_vector.hpp"));
    for (const char* word : {"malloc", "calloc", "realloc", "free"}) {
        if (!::zth::find_word(code, word).empty()) {
            ::zth::fail(std::string("int_vector.hpp uses ") + word + ": use new[] / delete[] instead");
            return;
        }
    }
    ZTH_EXPECT_TRUE(true);
}
ZTH_TEST("clean", "int_vector.hpp does not include a standard container: the point is to write the owner yourself") {
    const std::string raw = ::zth::read_file("int_vector.hpp");
    for (const char* header : {"<vector>", "<deque>", "<list>", "<forward_list>", "<array>", "<valarray>"}) {
        if (raw.find(header) != std::string::npos) {
            ::zth::fail(std::string("int_vector.hpp includes ") + header + ": the vector's storage has to be your own new[] block");
            return;
        }
    }
    ZTH_EXPECT_TRUE(true);
}
