#define ZTH_TRACK_ALLOCATIONS
#include <algorithm>
#include <cstddef>
#include <deque>
#include <iterator>
#include <memory>
#include <numeric>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

#include "ring_buffer.hpp"
#include "zth_test.hpp"

namespace {

using S = std::string;
using V = std::vector<int>;
template <std::size_t N> using IntRing = RingBuffer<int, N>;

struct NoDefault {  // not default constructible
    explicit NoDefault(int x) : v(x) {}
    int v;
};

struct Item {
    int id;
    S name;
    Item() : id(0) {}
    Item(int i, S n) : id(i), name(std::move(n)) {}
};

// ---- is RingBuffer<T, N> a valid type? (the constraints are checked here without a compile error) ----
template <class T, std::size_t N> constexpr bool valid_ring = requires { typename RingBuffer<T, N>; };

// the elements, oldest first, read with operator[] only (no iterators)
template <class B>
std::vector<typename B::value_type> by_index(const B& b) {
    std::vector<typename B::value_type> out;
    for (std::size_t i = 0; i < b.size(); ++i) out.push_back(b[i]);
    return out;
}

// the elements, oldest first, read with the iterators
template <class B>
std::vector<typename B::value_type> by_iterator(const B& b) {
    return std::vector<typename B::value_type>(b.begin(), b.end());
}

template <std::size_t N>
IntRing<N> filled(int count) {  // 1, 2, ..., count
    IntRing<N> b;
    for (int i = 1; i <= count; ++i) b.push(i);
    return b;
}

}  // namespace

// ================================================ Works =======================================================
ZTH_TEST("works", "capacity() is N, and a new buffer is empty and not full") {
    IntRing<4> b;
    constexpr std::size_t cap = IntRing<7>::capacity();
    ZTH_EXPECT_EQ(cap, std::size_t{7});
    ZTH_EXPECT_EQ(b.capacity(), std::size_t{4});
    ZTH_EXPECT_EQ(b.size(), std::size_t{0});
    ZTH_EXPECT_TRUE(b.empty());
    ZTH_EXPECT_FALSE(b.full());
}
ZTH_TEST("works", "push adds at the back: size grows and the buffer becomes full at N elements") {
    IntRing<3> b;
    ZTH_EXPECT_TRUE(b.push(10));
    ZTH_EXPECT_EQ(b.size(), std::size_t{1});
    ZTH_EXPECT_FALSE(b.empty());
    ZTH_EXPECT_FALSE(b.full());
    ZTH_EXPECT_TRUE(b.push(20));
    ZTH_EXPECT_TRUE(b.push(30));
    ZTH_EXPECT_EQ(b.size(), std::size_t{3});
    ZTH_EXPECT_TRUE(b.full());
    ZTH_EXPECT_TRUE((by_index(b) == V{10, 20, 30}));
}
ZTH_TEST("works", "push onto a full buffer is refused and changes nothing") {
    IntRing<2> b = filled<2>(2);
    ZTH_EXPECT_FALSE(b.push(99));
    int lvalue = 98;
    ZTH_EXPECT_FALSE(b.push(lvalue));
    ZTH_EXPECT_EQ(b.size(), std::size_t{2});
    ZTH_EXPECT_TRUE((by_index(b) == V{1, 2}));
}
ZTH_TEST("works", "pop returns the OLDEST element (first in, first out)") {
    IntRing<4> b = filled<4>(3);
    auto a = b.pop();
    ZTH_EXPECT_TRUE(a.has_value());
    if (a) ZTH_EXPECT_EQ(*a, 1);
    auto c = b.pop();
    if (c) ZTH_EXPECT_EQ(*c, 2);
    ZTH_EXPECT_EQ(b.size(), std::size_t{1});
    ZTH_EXPECT_EQ(b.front(), 3);
}
ZTH_TEST("works", "pop on an empty buffer gives an empty optional") {
    IntRing<3> b;
    ZTH_EXPECT_FALSE(b.pop().has_value());
    b.push(1);
    b.pop();
    ZTH_EXPECT_FALSE(b.pop().has_value());
    ZTH_EXPECT_TRUE(b.empty());
}
ZTH_TEST("works", "front, back and operator[] (index 0 is the oldest)") {
    IntRing<5> b = filled<5>(4);
    ZTH_EXPECT_EQ(b.front(), 1);
    ZTH_EXPECT_EQ(b.back(), 4);
    ZTH_EXPECT_EQ(b[0], 1);
    ZTH_EXPECT_EQ(b[2], 3);
    ZTH_EXPECT_EQ(b[3], 4);
    const IntRing<5>& cb = b;
    ZTH_EXPECT_EQ(cb.front(), 1);
    ZTH_EXPECT_EQ(cb.back(), 4);
    ZTH_EXPECT_EQ(cb[1], 2);
}
ZTH_TEST("works", "the buffer wraps around: slots are reused after pops") {
    IntRing<4> b = filled<4>(4);
    b.pop();
    b.pop();
    ZTH_EXPECT_TRUE(b.push(5));
    ZTH_EXPECT_TRUE(b.push(6));
    ZTH_EXPECT_TRUE(b.full());
    ZTH_EXPECT_FALSE(b.push(7));
    ZTH_EXPECT_TRUE((by_index(b) == V{3, 4, 5, 6}));
    ZTH_EXPECT_EQ(b.front(), 3);
    ZTH_EXPECT_EQ(b.back(), 6);
}
ZTH_TEST("works", "push_overwrite behaves like push while there is room") {
    IntRing<3> b;
    b.push_overwrite(1);
    b.push_overwrite(2);
    ZTH_EXPECT_EQ(b.size(), std::size_t{2});
    ZTH_EXPECT_TRUE((by_index(b) == V{1, 2}));
}
ZTH_TEST("works", "push_overwrite on a full buffer replaces the oldest element") {
    IntRing<3> b = filled<3>(3);
    b.push_overwrite(4);
    ZTH_EXPECT_EQ(b.size(), std::size_t{3});
    ZTH_EXPECT_TRUE((by_index(b) == V{2, 3, 4}));
    b.push_overwrite(5);
    b.push_overwrite(6);
    ZTH_EXPECT_TRUE((by_index(b) == V{4, 5, 6}));
    ZTH_EXPECT_EQ(b.front(), 4);
    ZTH_EXPECT_EQ(b.back(), 6);
}
ZTH_TEST("works", "emplace builds the element from constructor arguments") {
    RingBuffer<Item, 3> b;
    ZTH_EXPECT_TRUE(b.emplace(7, S("seven")));
    ZTH_EXPECT_TRUE(b.emplace(8, "eight"));
    ZTH_EXPECT_EQ(b.size(), std::size_t{2});
    ZTH_EXPECT_EQ(b[0].id, 7);
    ZTH_EXPECT_EQ(b[0].name, S("seven"));
    ZTH_EXPECT_EQ(b[1].name, S("eight"));
    RingBuffer<S, 2> words;
    ZTH_EXPECT_TRUE(words.emplace(3, 'x'));
    ZTH_EXPECT_EQ(words[0], S("xxx"));
}
ZTH_TEST("works", "emplace on a full buffer is refused") {
    RingBuffer<S, 1> b;
    ZTH_EXPECT_TRUE(b.emplace(2, 'a'));
    ZTH_EXPECT_FALSE(b.emplace(2, 'b'));
    ZTH_EXPECT_EQ(b.size(), std::size_t{1});
    ZTH_EXPECT_EQ(b.front(), S("aa"));
}
ZTH_TEST("works", "clear empties the buffer, which can then be used again") {
    IntRing<3> b = filled<3>(3);
    b.clear();
    ZTH_EXPECT_TRUE(b.empty());
    ZTH_EXPECT_EQ(b.size(), std::size_t{0});
    ZTH_EXPECT_FALSE(b.pop().has_value());
    b.push(10);
    b.push(20);
    ZTH_EXPECT_TRUE((by_index(b) == V{10, 20}));
}
ZTH_TEST("works", "range-for visits the elements oldest first") {
    IntRing<4> b = filled<4>(3);
    V seen;
    for (int x : b) seen.push_back(x);
    ZTH_EXPECT_TRUE((seen == V{1, 2, 3}));
}
ZTH_TEST("works", "iteration follows the logical order after the buffer has wrapped") {
    IntRing<4> b = filled<4>(4);
    b.pop();
    b.pop();
    b.push(5);
    b.push(6);
    ZTH_EXPECT_TRUE((by_iterator(b) == V{3, 4, 5, 6}));
}
ZTH_TEST("works", "begin() == end() for an empty buffer, and end() is one past the last element") {
    IntRing<3> b;
    ZTH_EXPECT_TRUE(b.begin() == b.end());
    b.push(1);
    ZTH_EXPECT_FALSE(b.begin() == b.end());
    ZTH_EXPECT_TRUE(b.begin() != b.end());
    auto it = b.begin();
    ++it;
    ZTH_EXPECT_TRUE(it == b.end());
}
ZTH_TEST("works", "iteration through a const buffer, and writing through a non-const iterator") {
    IntRing<3> b = filled<3>(3);
    for (int& x : b) x *= 10;
    const IntRing<3>& cb = b;
    V seen;
    for (const int& x : cb) seen.push_back(x);
    ZTH_EXPECT_TRUE((seen == V{10, 20, 30}));
}
ZTH_TEST("works", "the standard algorithms work on it (it is a real forward range)") {
    IntRing<5> b = filled<5>(4);
    ZTH_EXPECT_EQ(std::accumulate(b.begin(), b.end(), 0), 10);
    ZTH_EXPECT_EQ(std::distance(b.begin(), b.end()), 4);
    ZTH_EXPECT_TRUE(std::find(b.begin(), b.end(), 3) != b.end());
    ZTH_EXPECT_TRUE(std::find(b.begin(), b.end(), 9) == b.end());
    ZTH_EXPECT_EQ(std::count_if(b.begin(), b.end(), [](int x) { return x % 2 == 0; }), 2);
    ZTH_EXPECT_TRUE((V(b.begin(), b.end()) == V{1, 2, 3, 4}));
}
ZTH_TEST("works", "operator-> reaches the members of the element") {
    RingBuffer<Item, 2> b;
    b.emplace(5, "five");
    ZTH_EXPECT_EQ(b.begin()->id, 5);
    ZTH_EXPECT_EQ(b.begin()->name.size(), std::size_t{4});
}
ZTH_TEST("works", "it holds std::string and a counting type too") {
    RingBuffer<S, 3> words;
    words.push("a");
    words.push(S("b"));
    S c = "c";
    words.push(c);
    ZTH_EXPECT_TRUE((by_index(words) == std::vector<S>{"a", "b", "c"}));
    RingBuffer<zth::Tracked, 3> t;
    t.push(zth::Tracked(1));
    t.push(zth::Tracked(2));
    ZTH_EXPECT_EQ(t.front().value, 1);
    ZTH_EXPECT_EQ(t.back().value, 2);
}
ZTH_TEST("works", "the constraints: N must be positive and T default constructible") {
    ZTH_EXPECT_TRUE((valid_ring<int, 4>));
    ZTH_EXPECT_TRUE((valid_ring<S, 1>));
    ZTH_EXPECT_FALSE((valid_ring<int, 0>));
    ZTH_EXPECT_FALSE((valid_ring<NoDefault, 3>));
}

// ================================================ Robust ======================================================
ZTH_TEST("robust", "a buffer of capacity 1") {
    IntRing<1> b;
    ZTH_EXPECT_TRUE(b.push(1));
    ZTH_EXPECT_TRUE(b.full());
    ZTH_EXPECT_FALSE(b.push(2));
    ZTH_EXPECT_EQ(b.front(), 1);
    ZTH_EXPECT_EQ(b.back(), 1);
    b.push_overwrite(3);
    ZTH_EXPECT_EQ(b.size(), std::size_t{1});
    ZTH_EXPECT_EQ(b.front(), 3);
    auto x = b.pop();
    ZTH_EXPECT_TRUE(x.has_value());
    ZTH_EXPECT_TRUE(b.empty());
    ZTH_EXPECT_TRUE(b.push(4));
    ZTH_EXPECT_EQ(b.front(), 4);
}
ZTH_TEST("robust", "push and pop in turn, many times around the buffer") {
    IntRing<3> b;
    long sum = 0;
    for (int i = 1; i <= 1000; ++i) {
        ZTH_EXPECT_TRUE(b.push(i));
        auto x = b.pop();
        ZTH_EXPECT_TRUE(x.has_value());
        if (x) sum += *x;
    }
    ZTH_EXPECT_EQ(sum, 500500L);
    ZTH_EXPECT_TRUE(b.empty());
}
ZTH_TEST("robust", "random operations agree with a std::deque (the model)") {
    zth::Lcg rng(20240607);
    IntRing<5> b;
    std::deque<int> model;
    for (int step = 0; step < 20000; ++step) {
        const int op = rng.below(10);
        const int value = rng.below(1000);
        if (op < 4) {
            const bool ok = b.push(value);
            if (model.size() < 5) {
                model.push_back(value);
                ZTH_EXPECT_TRUE(ok);
            } else {
                ZTH_EXPECT_FALSE(ok);
            }
        } else if (op < 7) {
            auto got = b.pop();
            if (model.empty()) {
                ZTH_EXPECT_FALSE(got.has_value());
            } else {
                ZTH_EXPECT_TRUE(got.has_value());
                if (got) ZTH_EXPECT_EQ(*got, model.front());
                model.pop_front();
            }
        } else if (op < 9) {
            b.push_overwrite(value);
            if (model.size() == 5) model.pop_front();
            model.push_back(value);
        } else if (rng.below(8) == 0) {
            b.clear();
            model.clear();
        }
        ZTH_EXPECT_EQ(b.size(), model.size());
        if (!model.empty()) {
            ZTH_EXPECT_EQ(b.front(), model.front());
            ZTH_EXPECT_EQ(b.back(), model.back());
        }
        if (step % 50 == 0) ZTH_EXPECT_TRUE((by_iterator(b) == V(model.begin(), model.end())));
    }
}
ZTH_TEST("robust", "clear lets go of what the elements held (a shared_ptr is released)") {
    auto p = std::make_shared<int>(5);
    RingBuffer<std::shared_ptr<int>, 3> b;
    b.push(p);
    b.push(p);
    ZTH_EXPECT_EQ(p.use_count(), 3L);
    b.clear();
    ZTH_EXPECT_EQ(p.use_count(), 1L);
}
ZTH_TEST("robust", "pop hands the element over: the buffer keeps no second owner") {
    auto p = std::make_shared<int>(5);
    RingBuffer<std::shared_ptr<int>, 3> b;
    b.push(p);
    ZTH_EXPECT_EQ(p.use_count(), 2L);
    {
        auto out = b.pop();
        ZTH_EXPECT_TRUE(out.has_value());
        ZTH_EXPECT_EQ(p.use_count(), 2L);  // the optional owns it now, the buffer's slot was moved from
    }
    ZTH_EXPECT_EQ(p.use_count(), 1L);
}
ZTH_TEST("robust", "a move-only element type works (push by rvalue, pop)") {
    RingBuffer<std::unique_ptr<int>, 3> b;
    ZTH_EXPECT_TRUE(b.push(std::make_unique<int>(1)));
    ZTH_EXPECT_TRUE(b.emplace(new int(2)));
    ZTH_EXPECT_EQ(*b.front(), 1);
    auto a = b.pop();
    ZTH_EXPECT_TRUE(a.has_value());
    if (a) ZTH_EXPECT_EQ(**a, 1);
    auto c = b.pop();
    if (c) ZTH_EXPECT_EQ(**c, 2);
    ZTH_EXPECT_TRUE(b.empty());
}
ZTH_TEST("robust", "copying a buffer copies its elements: the copies are independent") {
    IntRing<4> a = filled<4>(3);
    IntRing<4> b = a;
    a.pop();
    a.push(9);
    b.front() = 100;
    ZTH_EXPECT_TRUE((by_index(a) == V{2, 3, 9}));
    ZTH_EXPECT_TRUE((by_index(b) == V{100, 2, 3}));
    IntRing<4> c;
    c.push(7);
    c = b;
    ZTH_EXPECT_TRUE((by_index(c) == V{100, 2, 3}));
    b.pop();
    ZTH_EXPECT_EQ(c.size(), std::size_t{3});
}
ZTH_TEST("robust", "a copy of a WRAPPED buffer reads the same") {
    IntRing<3> a = filled<3>(3);
    a.pop();
    a.push(4);
    IntRing<3> b = a;
    ZTH_EXPECT_TRUE((by_index(b) == V{2, 3, 4}));
    ZTH_EXPECT_TRUE((by_iterator(b) == V{2, 3, 4}));
}
ZTH_TEST("robust", "front() and back() give modifiable references") {
    IntRing<3> b = filled<3>(3);
    b.front() = 10;
    b.back() = 30;
    b[1] = 20;
    ZTH_EXPECT_TRUE((by_index(b) == V{10, 20, 30}));
}
ZTH_TEST("robust", "the iterators are forward iterators; iterator converts to const_iterator (not the other way)") {
    using It = IntRing<3>::iterator;
    using CIt = IntRing<3>::const_iterator;
    ZTH_EXPECT_TRUE(std::forward_iterator<It>);
    ZTH_EXPECT_TRUE(std::forward_iterator<CIt>);
    ZTH_EXPECT_TRUE((std::is_convertible_v<It, CIt>));
    ZTH_EXPECT_FALSE((std::is_convertible_v<CIt, It>));
    ZTH_EXPECT_TRUE((std::is_same_v<std::iter_reference_t<It>, int&>));
    ZTH_EXPECT_TRUE((std::is_same_v<std::iter_reference_t<CIt>, const int&>));
}
ZTH_TEST("robust", "post-increment returns the old position; iterators from the same buffer compare equal") {
    IntRing<3> b = filled<3>(3);
    auto it = b.begin();
    auto old = it++;
    ZTH_EXPECT_EQ(*old, 1);
    ZTH_EXPECT_EQ(*it, 2);
    auto it2 = b.begin();
    ++it2;
    ZTH_EXPECT_TRUE(it == it2);
    ZTH_EXPECT_TRUE(b.cbegin() == std::as_const(b).begin());
    ZTH_EXPECT_TRUE(b.cend() == std::as_const(b).end());
}
ZTH_TEST("robust", "a full buffer iterates all N elements and then stops") {
    IntRing<4> b = filled<4>(4);
    b.pop();
    b.push(5);
    ZTH_EXPECT_EQ(std::distance(b.begin(), b.end()), 4);
    ZTH_EXPECT_TRUE((by_iterator(b) == V{2, 3, 4, 5}));
}
ZTH_TEST("robust", "strings keep their values when the buffer wraps") {
    RingBuffer<S, 2> b;
    b.push(S(40, 'a'));
    b.push(S(40, 'b'));
    auto first = b.pop();
    b.push(S(40, 'c'));
    ZTH_EXPECT_TRUE(first.has_value());
    if (first) ZTH_EXPECT_EQ(*first, S(40, 'a'));
    ZTH_EXPECT_EQ(b[0], S(40, 'b'));
    ZTH_EXPECT_EQ(b[1], S(40, 'c'));
}

// ================================================ Fast ========================================================
ZTH_TEST("fast", "no heap allocation at all: the elements live inside the object") {
    zth::mem::Scope scope;
    IntRing<64> b;
    for (int i = 0; i < 200; ++i) {
        b.push_overwrite(i);
        if (i % 3 == 0) b.pop();
    }
    long sum = 0;
    for (int x : b) sum += x;
    b.clear();
    ZTH_EXPECT_TRUE(sum > 0);
    ZTH_EXPECT_ALLOCS_AT_MOST(scope, 0);
}
ZTH_TEST("fast", "the object is not bigger than N elements plus a few indices (no pointer to a heap block)") {
    ZTH_EXPECT_TRUE(sizeof(IntRing<8>) <= 8 * sizeof(int) + 3 * sizeof(std::size_t));
    ZTH_EXPECT_TRUE(sizeof(RingBuffer<double, 4>) <= 4 * sizeof(double) + 3 * sizeof(std::size_t));
}
ZTH_TEST("fast", "push of an rvalue moves into the slot: no copies") {
    RingBuffer<zth::Tracked, 3> b;
    zth::Tracked::reset();
    b.push(zth::Tracked(1));
    ZTH_EXPECT_EQ(zth::Tracked::copies, 0);
    ZTH_EXPECT_EQ(zth::Tracked::copy_assigns, 0);
    ZTH_EXPECT_EQ(zth::Tracked::moves, 0);  // assigned (move assignment), not constructed
    ZTH_EXPECT_EQ(zth::Tracked::move_assigns, 1);
}
ZTH_TEST("fast", "push of an lvalue copies exactly once, with no temporary") {
    RingBuffer<zth::Tracked, 3> b;
    zth::Tracked x(7);
    zth::Tracked::reset();
    b.push(x);
    ZTH_EXPECT_EQ(zth::Tracked::copy_assigns, 1);
    ZTH_EXPECT_EQ(zth::Tracked::copies, 0);
    ZTH_EXPECT_EQ(zth::Tracked::moves, 0);
    ZTH_EXPECT_EQ(zth::Tracked::move_assigns, 0);
    ZTH_EXPECT_EQ(x.value, 7);
}
ZTH_TEST("fast", "pop moves the element out exactly once and never copies it") {
    RingBuffer<zth::Tracked, 3> b;
    b.push(zth::Tracked(5));
    zth::Tracked::reset();
    auto out = b.pop();
    ZTH_EXPECT_TRUE(out.has_value());
    ZTH_EXPECT_EQ(zth::Tracked::copies, 0);
    ZTH_EXPECT_EQ(zth::Tracked::copy_assigns, 0);
    ZTH_EXPECT_EQ(zth::Tracked::moves, 1);
    if (out) ZTH_EXPECT_EQ(out->value, 5);
}
ZTH_TEST("fast", "emplace builds one temporary and moves it into the slot: no copies") {
    RingBuffer<zth::Tracked, 3> b;
    zth::Tracked::reset();
    b.emplace(4);
    ZTH_EXPECT_EQ(zth::Tracked::copies, 0);
    ZTH_EXPECT_EQ(zth::Tracked::copy_assigns, 0);
    ZTH_EXPECT_EQ(zth::Tracked::value_ctors, 1);
    ZTH_EXPECT_EQ(b.front().value, 4);
}
ZTH_TEST("fast", "emplace forwards an rvalue argument: it is moved, never copied") {
    RingBuffer<zth::Tracked, 3> b;
    zth::Tracked::reset();
    b.emplace(zth::Tracked(9));
    ZTH_EXPECT_EQ(zth::Tracked::copies, 0);
    ZTH_EXPECT_EQ(zth::Tracked::copy_assigns, 0);
    ZTH_EXPECT_EQ(b.front().value, 9);
}
ZTH_TEST("fast", "push_overwrite of an rvalue copies nothing") {
    RingBuffer<zth::Tracked, 2> b;
    b.push(zth::Tracked(1));
    b.push(zth::Tracked(2));
    zth::Tracked::reset();
    b.push_overwrite(zth::Tracked(3));
    ZTH_EXPECT_EQ(zth::Tracked::copies, 0);
    ZTH_EXPECT_EQ(zth::Tracked::copy_assigns, 0);
    ZTH_EXPECT_EQ(b.front().value, 2);
    ZTH_EXPECT_EQ(b.back().value, 3);
}
ZTH_TEST("fast", "front, back, operator[], size and iteration copy and move nothing") {
    RingBuffer<zth::Tracked, 4> b;
    b.push(zth::Tracked(1));
    b.push(zth::Tracked(2));
    b.push(zth::Tracked(3));
    zth::Tracked::reset();
    int sum = b.front().value + b.back().value + b[1].value;
    for (const auto& t : b) sum += t.value;
    sum += static_cast<int>(b.size());
    ZTH_EXPECT_EQ(sum, 1 + 3 + 2 + 6 + 3);
    ZTH_EXPECT_EQ(zth::Tracked::constructed(), 0);
    ZTH_EXPECT_EQ(zth::Tracked::copy_assigns + zth::Tracked::move_assigns, 0);
}

// ================================================ Clean =======================================================
ZTH_TEST("clean", "ring_buffer.hpp does not use new, delete, malloc or free: std::array holds the elements") {
    ZTH_EXPECT_NO_RAW_MEMORY("ring_buffer.hpp");
}
ZTH_TEST("clean", "ring_buffer.hpp does not include a dynamic container") {
    const std::string raw = ::zth::read_file("ring_buffer.hpp");
    for (const char* header : {"<vector>", "<deque>", "<list>", "<forward_list>", "<map>", "<queue>"}) {
        if (raw.find(header) != std::string::npos) {
            ::zth::fail(std::string("ring_buffer.hpp includes ") + header + ": the storage is a std::array<T, N> inside the object");
            return;
        }
    }
    ZTH_EXPECT_TRUE(true);
}
ZTH_TEST("clean", "ring_buffer.hpp has no `using namespace`") {
    const auto layout = ::zth::source_layout_of("ring_buffer.hpp");
    ZTH_EXPECT_FALSE(layout.using_directive);
}
