#define ZTH_TRACK_ALLOCATIONS
#include <algorithm>
#include <cctype>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <numeric>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

#include "static_vector.hpp"
#include "zth_test.hpp"

namespace {

using S = std::string;
using V = std::vector<int>;
template <std::size_t N> using IntVec = StaticVector<int, N>;

struct NoDefault {  // not default constructible, not copyable
    explicit NoDefault(int x) : v(x) {}
    NoDefault(const NoDefault&) = default;
    NoDefault& operator=(const NoDefault&) = default;
    int v;
};

struct Item {
    int id;
    S name;
    Item(int i, S n) : id(i), name(std::move(n)) {}
};

// counts how many objects are alive (constructed - destroyed), whatever kind of constructor made them
struct Life {
    static inline int alive = 0;
    int id;
    explicit Life(int i) : id(i) { ++alive; }
    Life(const Life& o) : id(o.id) { ++alive; }
    Life(Life&& o) noexcept : id(o.id) {
        o.id = -1;
        ++alive;
    }
    Life& operator=(const Life&) = default;
    Life& operator=(Life&& o) noexcept {
        id = o.id;
        o.id = -1;
        return *this;
    }
    ~Life() { --alive; }
};

template <class T, std::size_t N> constexpr bool valid_vec = requires { typename StaticVector<T, N>; };

template <class B>
std::vector<typename B::value_type> contents(const B& b) {
    std::vector<typename B::value_type> out;
    for (std::size_t i = 0; i < b.size(); ++i) out.push_back(b[i]);
    return out;
}

template <std::size_t N>
IntVec<N> filled(int count) {  // 1, 2, ..., count
    IntVec<N> v;
    for (int i = 1; i <= count; ++i) v.push_back(i);
    return v;
}

}  // namespace

// ================================================ Works =======================================================
ZTH_TEST("works", "capacity() is N, and a new vector is empty and not full") {
    IntVec<4> v;
    constexpr std::size_t cap = IntVec<9>::capacity();
    ZTH_EXPECT_EQ(cap, std::size_t{9});
    ZTH_EXPECT_EQ(v.capacity(), std::size_t{4});
    ZTH_EXPECT_EQ(v.size(), std::size_t{0});
    ZTH_EXPECT_TRUE(v.empty());
    ZTH_EXPECT_FALSE(v.full());
}
ZTH_TEST("works", "push_back adds at the end; operator[], front and back read the elements") {
    IntVec<4> v;
    ZTH_EXPECT_TRUE(v.push_back(10));
    ZTH_EXPECT_TRUE(v.push_back(20));
    int x = 30;
    ZTH_EXPECT_TRUE(v.push_back(x));
    ZTH_EXPECT_EQ(v.size(), std::size_t{3});
    ZTH_EXPECT_FALSE(v.empty());
    ZTH_EXPECT_EQ(v[0], 10);
    ZTH_EXPECT_EQ(v[2], 30);
    ZTH_EXPECT_EQ(v.front(), 10);
    ZTH_EXPECT_EQ(v.back(), 30);
    const IntVec<4>& cv = v;
    ZTH_EXPECT_EQ(cv[1], 20);
    ZTH_EXPECT_EQ(cv.front(), 10);
    ZTH_EXPECT_EQ(cv.back(), 30);
}
ZTH_TEST("works", "push_back onto a full vector is refused and changes nothing") {
    IntVec<2> v = filled<2>(2);
    ZTH_EXPECT_TRUE(v.full());
    ZTH_EXPECT_FALSE(v.push_back(3));
    int x = 4;
    ZTH_EXPECT_FALSE(v.push_back(x));
    ZTH_EXPECT_FALSE(v.emplace_back(5));
    ZTH_EXPECT_EQ(v.size(), std::size_t{2});
    ZTH_EXPECT_TRUE((contents(v) == V{1, 2}));
}
ZTH_TEST("works", "pop_back removes the last element; on an empty vector it does nothing") {
    IntVec<3> v = filled<3>(3);
    v.pop_back();
    ZTH_EXPECT_EQ(v.size(), std::size_t{2});
    ZTH_EXPECT_EQ(v.back(), 2);
    v.pop_back();
    v.pop_back();
    ZTH_EXPECT_TRUE(v.empty());
    v.pop_back();
    ZTH_EXPECT_TRUE(v.empty());
    ZTH_EXPECT_TRUE(v.push_back(9));
    ZTH_EXPECT_EQ(v.front(), 9);
}
ZTH_TEST("works", "the initializer-list constructor") {
    IntVec<5> v{3, 1, 2};
    ZTH_EXPECT_TRUE((contents(v) == V{3, 1, 2}));
    IntVec<2> small{7, 8, 9, 10};
    ZTH_EXPECT_TRUE((contents(small) == V{7, 8}));  // the first N are kept
    IntVec<3> none{};
    ZTH_EXPECT_TRUE(none.empty());
}
ZTH_TEST("works", "emplace_back builds the element in place from constructor arguments") {
    StaticVector<Item, 3> v;
    ZTH_EXPECT_TRUE(v.emplace_back(7, S("seven")));
    ZTH_EXPECT_TRUE(v.emplace_back(8, "eight"));
    ZTH_EXPECT_EQ(v.size(), std::size_t{2});
    ZTH_EXPECT_EQ(v[0].id, 7);
    ZTH_EXPECT_EQ(v[0].name, S("seven"));
    ZTH_EXPECT_EQ(v[1].name, S("eight"));
    StaticVector<S, 2> words;
    ZTH_EXPECT_TRUE(words.emplace_back(3, 'x'));
    ZTH_EXPECT_EQ(words[0], S("xxx"));
}
ZTH_TEST("works", "clear removes everything, and the vector can be used again") {
    IntVec<3> v = filled<3>(3);
    v.clear();
    ZTH_EXPECT_TRUE(v.empty());
    ZTH_EXPECT_TRUE(v.push_back(5));
    ZTH_EXPECT_TRUE(v.push_back(6));
    ZTH_EXPECT_TRUE((contents(v) == V{5, 6}));
}
ZTH_TEST("works", "range-for and begin()/end() visit the elements in order; empty means begin() == end()") {
    IntVec<4> v = filled<4>(3);
    V seen;
    for (int x : v) seen.push_back(x);
    ZTH_EXPECT_TRUE((seen == V{1, 2, 3}));
    ZTH_EXPECT_EQ(v.end() - v.begin(), 3);
    IntVec<4> e;
    ZTH_EXPECT_TRUE(e.begin() == e.end());
    const IntVec<4>& cv = v;
    V cseen(cv.begin(), cv.end());
    ZTH_EXPECT_TRUE((cseen == V{1, 2, 3}));
}
ZTH_TEST("works", "the elements are contiguous (data() works like a C array)") {
    IntVec<5> v = filled<5>(4);
    ZTH_EXPECT_TRUE(v.data() == &v[0]);
    ZTH_EXPECT_TRUE(&v[3] == v.data() + 3);
    ZTH_EXPECT_TRUE(&v[1] == &v[0] + 1);
    ZTH_EXPECT_EQ(v.data()[2], 3);
}
ZTH_TEST("works", "the standard algorithms work on it") {
    IntVec<6> v{5, 3, 9, 1};
    std::sort(v.begin(), v.end());
    ZTH_EXPECT_TRUE((contents(v) == V{1, 3, 5, 9}));
    ZTH_EXPECT_EQ(std::accumulate(v.begin(), v.end(), 0), 18);
    ZTH_EXPECT_TRUE(std::find(v.begin(), v.end(), 5) != v.end());
    ZTH_EXPECT_EQ(std::count_if(v.begin(), v.end(), [](int x) { return x > 2; }), 3);
}
ZTH_TEST("works", "insert puts an element before the index (index == size appends)") {
    IntVec<6> v{1, 2, 3};
    ZTH_EXPECT_TRUE(v.insert(0, 10));
    ZTH_EXPECT_TRUE((contents(v) == V{10, 1, 2, 3}));
    ZTH_EXPECT_TRUE(v.insert(2, 20));
    ZTH_EXPECT_TRUE((contents(v) == V{10, 1, 20, 2, 3}));
    ZTH_EXPECT_TRUE(v.insert(5, 30));
    ZTH_EXPECT_TRUE((contents(v) == V{10, 1, 20, 2, 3, 30}));
}
ZTH_TEST("works", "insert is refused when the vector is full or the index is past the end") {
    IntVec<3> v{1, 2};
    ZTH_EXPECT_FALSE(v.insert(3, 9));
    ZTH_EXPECT_TRUE(v.insert(1, 9));
    ZTH_EXPECT_TRUE(v.full());
    ZTH_EXPECT_FALSE(v.insert(0, 5));
    ZTH_EXPECT_TRUE((contents(v) == V{1, 9, 2}));
}
ZTH_TEST("works", "erase removes the element at the index; the following elements move down") {
    IntVec<5> v{1, 2, 3, 4, 5};
    ZTH_EXPECT_TRUE(v.erase(0));
    ZTH_EXPECT_TRUE((contents(v) == V{2, 3, 4, 5}));
    ZTH_EXPECT_TRUE(v.erase(2));
    ZTH_EXPECT_TRUE((contents(v) == V{2, 3, 5}));
    ZTH_EXPECT_TRUE(v.erase(2));
    ZTH_EXPECT_TRUE((contents(v) == V{2, 3}));
}
ZTH_TEST("works", "erase is refused for an index outside the vector") {
    IntVec<3> v{1, 2};
    ZTH_EXPECT_FALSE(v.erase(2));
    ZTH_EXPECT_FALSE(v.erase(100));
    IntVec<3> e;
    ZTH_EXPECT_FALSE(e.erase(0));
    ZTH_EXPECT_TRUE((contents(v) == V{1, 2}));
}
ZTH_TEST("works", "copying makes an independent copy") {
    IntVec<4> a{1, 2, 3};
    IntVec<4> b = a;
    a[0] = 100;
    a.push_back(4);
    ZTH_EXPECT_TRUE((contents(b) == V{1, 2, 3}));
    IntVec<4> c;
    c.push_back(9);
    c = a;
    ZTH_EXPECT_TRUE((contents(c) == V{100, 2, 3, 4}));
    a.pop_back();
    ZTH_EXPECT_EQ(c.size(), std::size_t{4});
}
ZTH_TEST("works", "moving transfers the elements and leaves the source empty") {
    StaticVector<S, 3> a;
    a.push_back(S(40, 'a'));
    a.push_back(S(40, 'b'));
    StaticVector<S, 3> b = std::move(a);
    ZTH_EXPECT_EQ(b.size(), std::size_t{2});
    ZTH_EXPECT_EQ(b[0], S(40, 'a'));
    ZTH_EXPECT_EQ(b[1], S(40, 'b'));
    ZTH_EXPECT_TRUE(a.empty());
    StaticVector<S, 3> c;
    c.push_back("old");
    c = std::move(b);
    ZTH_EXPECT_EQ(c.size(), std::size_t{2});
    ZTH_EXPECT_EQ(c[1], S(40, 'b'));
    ZTH_EXPECT_TRUE(b.empty());
}
ZTH_TEST("works", "== compares sizes and elements") {
    IntVec<4> a{1, 2, 3};
    IntVec<4> b{1, 2, 3};
    IntVec<4> c{1, 2};
    IntVec<4> d{1, 2, 4};
    ZTH_EXPECT_TRUE(a == b);
    ZTH_EXPECT_FALSE(a == c);
    ZTH_EXPECT_FALSE(a == d);
    ZTH_EXPECT_TRUE(IntVec<4>{} == IntVec<4>{});
}
ZTH_TEST("works", "it holds strings (types that own memory)") {
    StaticVector<S, 4> v;
    v.push_back("short");
    v.push_back(S(100, 'x'));
    S lvalue(60, 'y');
    v.push_back(lvalue);
    v.insert(1, S(70, 'z'));
    v.erase(0);
    ZTH_EXPECT_EQ(v.size(), std::size_t{3});
    ZTH_EXPECT_EQ(v[0], S(70, 'z'));
    ZTH_EXPECT_EQ(v[1], S(100, 'x'));
    ZTH_EXPECT_EQ(v[2], S(60, 'y'));
    ZTH_EXPECT_EQ(lvalue, S(60, 'y'));
}
ZTH_TEST("works", "the element type need NOT be default constructible") {
    StaticVector<NoDefault, 3> v;
    v.emplace_back(1);
    v.push_back(NoDefault(2));
    NoDefault three(3);
    v.push_back(three);
    ZTH_EXPECT_EQ(v.size(), std::size_t{3});
    ZTH_EXPECT_EQ(v[0].v + v[1].v + v[2].v, 6);
}
ZTH_TEST("works", "the constraint: N must be positive") {
    ZTH_EXPECT_TRUE((valid_vec<int, 4>));
    ZTH_EXPECT_TRUE((valid_vec<NoDefault, 1>));
    ZTH_EXPECT_FALSE((valid_vec<int, 0>));
}

// ================================================ Robust ======================================================
ZTH_TEST("robust", "a vector of capacity 1") {
    IntVec<1> v;
    ZTH_EXPECT_TRUE(v.push_back(1));
    ZTH_EXPECT_TRUE(v.full());
    ZTH_EXPECT_FALSE(v.push_back(2));
    ZTH_EXPECT_FALSE(v.insert(0, 5));
    ZTH_EXPECT_TRUE(v.erase(0));
    ZTH_EXPECT_TRUE(v.empty());
    ZTH_EXPECT_TRUE(v.insert(0, 7));
    ZTH_EXPECT_EQ(v.front(), 7);
    ZTH_EXPECT_EQ(v.back(), 7);
}
ZTH_TEST("robust", "random operations agree with a std::vector (the model)") {
    zth::Lcg rng(777);
    StaticVector<S, 8> v;
    std::vector<S> model;
    for (int step = 0; step < 6000; ++step) {
        const int op = rng.below(12);
        const S value(static_cast<std::size_t>(rng.below(40)), static_cast<char>('a' + rng.below(26)));
        if (op < 4) {
            const bool had_room = model.size() < 8;
            const bool ok = v.push_back(value);
            if (had_room) model.push_back(value);
            ZTH_EXPECT_EQ(ok, had_room);
        } else if (op < 6) {
            v.pop_back();
            if (!model.empty()) model.pop_back();
        } else if (op < 8) {
            const std::size_t index = static_cast<std::size_t>(rng.below(10));
            const bool ok = v.insert(index, value);
            if (model.size() < 8 && index <= model.size()) {
                model.insert(model.begin() + static_cast<std::ptrdiff_t>(index), value);
                ZTH_EXPECT_TRUE(ok);
            } else {
                ZTH_EXPECT_FALSE(ok);
            }
        } else if (op < 10) {
            const std::size_t index = static_cast<std::size_t>(rng.below(10));
            const bool ok = v.erase(index);
            if (index < model.size()) {
                model.erase(model.begin() + static_cast<std::ptrdiff_t>(index));
                ZTH_EXPECT_TRUE(ok);
            } else {
                ZTH_EXPECT_FALSE(ok);
            }
        } else if (op == 10 && rng.below(6) == 0) {
            v.clear();
            model.clear();
        } else {
            StaticVector<S, 8> copy = v;  // a copy must agree too
            ZTH_EXPECT_TRUE(copy == v);
        }
        ZTH_EXPECT_EQ(v.size(), model.size());
        if (v.size() == model.size()) {
            for (std::size_t i = 0; i < model.size(); ++i) {
                if (!(v[i] == model[i])) {
                    ::zth::fail("element " + std::to_string(i) + " differs from the model after step " + std::to_string(step));
                    return;
                }
            }
        }
    }
}
ZTH_TEST("robust", "self-assignment (copy and move) leaves the vector intact") {
    StaticVector<S, 3> v;
    v.push_back(S(50, 'a'));
    v.push_back(S(50, 'b'));
    StaticVector<S, 3>& same = v;
    v = same;
    ZTH_EXPECT_EQ(v.size(), std::size_t{2});
    ZTH_EXPECT_EQ(v[0], S(50, 'a'));
    v = std::move(same);
    ZTH_EXPECT_EQ(v.size(), std::size_t{2});
    ZTH_EXPECT_EQ(v[1], S(50, 'b'));
}
ZTH_TEST("robust", "assigning over a vector that already holds elements replaces them") {
    StaticVector<S, 4> a;
    a.push_back("one");
    a.push_back("two");
    a.push_back("three");
    StaticVector<S, 4> b;
    b.push_back(S(30, 'x'));
    a = b;
    ZTH_EXPECT_EQ(a.size(), std::size_t{1});
    ZTH_EXPECT_EQ(a[0], S(30, 'x'));
    StaticVector<S, 4> big;
    big.push_back("p");
    big.push_back("q");
    big.push_back("r");
    big.push_back("s");
    a = big;
    ZTH_EXPECT_EQ(a.size(), std::size_t{4});
    ZTH_EXPECT_EQ(a[3], S("s"));
}
ZTH_TEST("robust", "elements are released when they are removed (shared_ptr use counts)") {
    auto p = std::make_shared<int>(1);
    {
        StaticVector<std::shared_ptr<int>, 4> v;
        v.push_back(p);
        v.push_back(p);
        v.push_back(p);
        ZTH_EXPECT_EQ(p.use_count(), 4L);
        v.pop_back();
        ZTH_EXPECT_EQ(p.use_count(), 3L);
        v.erase(0);
        ZTH_EXPECT_EQ(p.use_count(), 2L);
        v.clear();
        ZTH_EXPECT_EQ(p.use_count(), 1L);
        v.push_back(p);
        ZTH_EXPECT_EQ(p.use_count(), 2L);
    }
    ZTH_EXPECT_EQ(p.use_count(), 1L);  // the destructor released the last one
}
ZTH_TEST("robust", "a moved-from vector holds no elements: shared_ptr counts stay right") {
    auto p = std::make_shared<int>(1);
    StaticVector<std::shared_ptr<int>, 3> a;
    a.push_back(p);
    a.push_back(p);
    ZTH_EXPECT_EQ(p.use_count(), 3L);
    StaticVector<std::shared_ptr<int>, 3> b = std::move(a);
    ZTH_EXPECT_EQ(p.use_count(), 3L);
    b.clear();
    ZTH_EXPECT_EQ(p.use_count(), 1L);
}
ZTH_TEST("robust", "a move-only element type works: push_back, emplace_back, insert, erase and moving the vector") {
    StaticVector<std::unique_ptr<int>, 4> v;
    ZTH_EXPECT_TRUE(v.push_back(std::make_unique<int>(1)));
    ZTH_EXPECT_TRUE(v.emplace_back(new int(2)));
    ZTH_EXPECT_TRUE(v.insert(1, std::make_unique<int>(9)));
    ZTH_EXPECT_EQ(*v[0], 1);
    ZTH_EXPECT_EQ(*v[1], 9);
    ZTH_EXPECT_EQ(*v[2], 2);
    ZTH_EXPECT_TRUE(v.erase(0));
    StaticVector<std::unique_ptr<int>, 4> w = std::move(v);
    ZTH_EXPECT_EQ(w.size(), std::size_t{2});
    ZTH_EXPECT_EQ(*w[0], 9);
    ZTH_EXPECT_EQ(*w[1], 2);
    ZTH_EXPECT_TRUE(v.empty());
}
ZTH_TEST("robust", "every object that was constructed is destroyed, exactly once (no leak, no double destruction)") {
    Life::alive = 0;
    {
        StaticVector<Life, 6> v;
        for (int i = 0; i < 5; ++i) v.emplace_back(i);
        ZTH_EXPECT_EQ(Life::alive, 5);
        v.insert(2, Life(10));
        ZTH_EXPECT_EQ(Life::alive, 6);
        v.erase(0);
        ZTH_EXPECT_EQ(Life::alive, 5);
        v.pop_back();
        ZTH_EXPECT_EQ(Life::alive, 4);
        StaticVector<Life, 6> copy = v;
        ZTH_EXPECT_EQ(Life::alive, 8);
        StaticVector<Life, 6> moved = std::move(copy);
        ZTH_EXPECT_EQ(Life::alive, 8);
        moved = v;
        ZTH_EXPECT_EQ(Life::alive, 8);
        v.clear();
        ZTH_EXPECT_EQ(Life::alive, 4);
    }
    ZTH_EXPECT_EQ(Life::alive, 0);
}
ZTH_TEST("robust", "an insert in the middle of a vector that is one short of full") {
    IntVec<5> v{1, 2, 3, 4};
    ZTH_EXPECT_TRUE(v.insert(3, 99));
    ZTH_EXPECT_TRUE((contents(v) == V{1, 2, 3, 99, 4}));
    ZTH_EXPECT_TRUE(v.full());
}
ZTH_TEST("robust", "const access and iteration") {
    const IntVec<4> v{4, 5, 6};
    ZTH_EXPECT_EQ(v[1], 5);
    ZTH_EXPECT_EQ(v.back(), 6);
    int sum = 0;
    for (const int& x : v) sum += x;
    ZTH_EXPECT_EQ(sum, 15);
    ZTH_EXPECT_EQ(std::distance(v.cbegin(), v.cend()), 3);
}
ZTH_TEST("robust", "storage is aligned for the element type (a 16-byte aligned type)") {
    struct alignas(16) Wide {
        double a, b;
    };
    StaticVector<Wide, 3> v;
    v.push_back(Wide{1, 2});
    v.push_back(Wide{3, 4});
    ZTH_EXPECT_EQ(reinterpret_cast<std::uintptr_t>(&v[0]) % 16, std::uintptr_t{0});
    ZTH_EXPECT_EQ(reinterpret_cast<std::uintptr_t>(&v[1]) % 16, std::uintptr_t{0});
    ZTH_EXPECT_EQ(v[1].b, 4.0);
}

// ================================================ Fast ========================================================
ZTH_TEST("fast", "an empty StaticVector constructs NO elements (unlike std::array<T, N>)") {
    zth::Tracked::reset();
    {
        StaticVector<zth::Tracked, 100> v;
        ZTH_EXPECT_EQ(zth::Tracked::constructed(), 0);
        v.emplace_back(1);
        ZTH_EXPECT_EQ(zth::Tracked::constructed(), 1);
    }
    ZTH_EXPECT_EQ(zth::Tracked::live(), 0);
}
ZTH_TEST("fast", "the destructor destroys exactly the live elements, no more and no fewer") {
    zth::Tracked::reset();
    {
        StaticVector<zth::Tracked, 8> v;
        v.emplace_back(1);
        v.emplace_back(2);
        v.emplace_back(3);
        ZTH_EXPECT_EQ(zth::Tracked::destroyed, 0);
    }
    ZTH_EXPECT_EQ(zth::Tracked::destroyed, 3);
    ZTH_EXPECT_EQ(zth::Tracked::constructed(), 3);
}
ZTH_TEST("fast", "pop_back and clear destroy their elements immediately") {
    StaticVector<zth::Tracked, 5> v;
    v.emplace_back(1);
    v.emplace_back(2);
    v.emplace_back(3);
    zth::Tracked::reset();
    v.pop_back();
    ZTH_EXPECT_EQ(zth::Tracked::destroyed, 1);
    v.clear();
    ZTH_EXPECT_EQ(zth::Tracked::destroyed, 3);
    ZTH_EXPECT_EQ(zth::Tracked::moves + zth::Tracked::copies + zth::Tracked::move_assigns + zth::Tracked::copy_assigns, 0);
}
ZTH_TEST("fast", "emplace_back constructs in place: one construction, no copy and no move") {
    StaticVector<zth::Tracked, 3> v;
    zth::Tracked::reset();
    v.emplace_back(5);
    ZTH_EXPECT_EQ(zth::Tracked::value_ctors, 1);
    ZTH_EXPECT_EQ(zth::Tracked::copies, 0);
    ZTH_EXPECT_EQ(zth::Tracked::moves, 0);
    ZTH_EXPECT_EQ(zth::Tracked::move_assigns + zth::Tracked::copy_assigns, 0);
}
ZTH_TEST("fast", "push_back of an rvalue is one move construction; of an lvalue one copy construction") {
    StaticVector<zth::Tracked, 3> v;
    zth::Tracked x(1);
    zth::Tracked::reset();
    v.push_back(zth::Tracked(2));
    ZTH_EXPECT_EQ(zth::Tracked::copies, 0);
    ZTH_EXPECT_EQ(zth::Tracked::moves, 1);
    zth::Tracked::reset();
    v.push_back(x);
    ZTH_EXPECT_EQ(zth::Tracked::copies, 1);
    ZTH_EXPECT_EQ(zth::Tracked::moves, 0);
    ZTH_EXPECT_EQ(zth::Tracked::copy_assigns + zth::Tracked::move_assigns, 0);  // slots are raw memory: nothing to assign to
}
ZTH_TEST("fast", "erase moves the following elements down (assignment) and destroys one") {
    StaticVector<zth::Tracked, 6> v;
    for (int i = 1; i <= 4; ++i) v.emplace_back(i);
    zth::Tracked::reset();
    ZTH_EXPECT_TRUE(v.erase(0));
    ZTH_EXPECT_EQ(zth::Tracked::move_assigns, 3);
    ZTH_EXPECT_EQ(zth::Tracked::destroyed, 1);
    ZTH_EXPECT_EQ(zth::Tracked::copies + zth::Tracked::copy_assigns, 0);
    ZTH_EXPECT_EQ(v.front().value, 2);
    ZTH_EXPECT_EQ(v.back().value, 4);
}
ZTH_TEST("fast", "insert in front of 3 elements: one move construction into the raw slot, the rest are move assignments, no copies") {
    StaticVector<zth::Tracked, 6> v;
    for (int i = 1; i <= 3; ++i) v.emplace_back(i);
    zth::Tracked::reset();
    ZTH_EXPECT_TRUE(v.insert(0, zth::Tracked(9)));
    ZTH_EXPECT_EQ(zth::Tracked::copies + zth::Tracked::copy_assigns, 0);
    ZTH_EXPECT_EQ(zth::Tracked::moves, 1);
    ZTH_EXPECT_EQ(zth::Tracked::move_assigns, 3);
    ZTH_EXPECT_EQ(zth::Tracked::destroyed, 1);  // only the temporary argument
    ZTH_EXPECT_EQ(v.front().value, 9);
    ZTH_EXPECT_EQ(v.back().value, 3);
}
ZTH_TEST("fast", "copy construction: one copy per element, nothing else") {
    StaticVector<zth::Tracked, 6> v;
    for (int i = 1; i <= 4; ++i) v.emplace_back(i);
    zth::Tracked::reset();
    StaticVector<zth::Tracked, 6> copy = v;
    ZTH_EXPECT_EQ(zth::Tracked::copies, 4);
    ZTH_EXPECT_EQ(zth::Tracked::defaults + zth::Tracked::value_ctors + zth::Tracked::moves, 0);
    ZTH_EXPECT_EQ(zth::Tracked::copy_assigns + zth::Tracked::move_assigns, 0);
}
ZTH_TEST("fast", "move construction: one move per element, no copies, and the source is emptied") {
    StaticVector<zth::Tracked, 6> v;
    for (int i = 1; i <= 4; ++i) v.emplace_back(i);
    zth::Tracked::reset();
    StaticVector<zth::Tracked, 6> moved = std::move(v);
    ZTH_EXPECT_EQ(zth::Tracked::moves, 4);
    ZTH_EXPECT_EQ(zth::Tracked::copies, 0);
    ZTH_EXPECT_EQ(zth::Tracked::destroyed, 4);  // the moved-from elements of the source
    ZTH_EXPECT_TRUE(v.empty());
}
ZTH_TEST("fast", "no heap allocation at all") {
    zth::mem::Scope scope;
    StaticVector<int, 64> v;
    for (int i = 0; i < 64; ++i) v.push_back(i);
    v.insert(10, 5);
    v.erase(0);
    v.pop_back();
    StaticVector<int, 64> copy = v;
    StaticVector<int, 64> moved = std::move(copy);
    long sum = 0;
    for (int x : moved) sum += x;
    ZTH_EXPECT_TRUE(sum > 0);
    ZTH_EXPECT_ALLOCS_AT_MOST(scope, 0);
}
ZTH_TEST("fast", "the object is N elements plus one counter (no pointer to a heap block)") {
    ZTH_EXPECT_TRUE(sizeof(IntVec<8>) <= 8 * sizeof(int) + sizeof(std::size_t));
    ZTH_EXPECT_TRUE(sizeof(StaticVector<double, 5>) <= 5 * sizeof(double) + 2 * sizeof(std::size_t));  // (+ padding)
    ZTH_EXPECT_TRUE(sizeof(StaticVector<char, 3>) <= 3 + 2 * sizeof(std::size_t));
}
ZTH_TEST("fast", "access and iteration copy and construct nothing") {
    StaticVector<zth::Tracked, 4> v;
    for (int i = 1; i <= 3; ++i) v.emplace_back(i);
    zth::Tracked::reset();
    int sum = v.front().value + v.back().value + v[1].value;
    for (const auto& t : v) sum += t.value;
    sum += static_cast<int>(v.size());
    ZTH_EXPECT_EQ(sum, 1 + 3 + 2 + 6 + 3);
    ZTH_EXPECT_EQ(zth::Tracked::constructed(), 0);
    ZTH_EXPECT_EQ(zth::Tracked::destroyed, 0);
}

// ================================================ Clean =======================================================
ZTH_TEST("clean", "static_vector.hpp does not use malloc, free, delete or `new T`: only placement new") {
    const std::string code = ::zth::strip_comments_and_literals(::zth::read_file("static_vector.hpp"));
    for (const char* word : {"malloc", "calloc", "realloc", "free", "delete"}) {
        for (std::size_t p : ::zth::find_word(code, word)) {
            // `= delete;` is fine
            std::size_t q = p;
            while (q > 0 && std::isspace(static_cast<unsigned char>(code[q - 1]))) --q;
            if (std::string(word) == "delete" && q > 0 && code[q - 1] == '=') continue;
            ::zth::fail(std::string("static_vector.hpp uses ") + word + ": the memory is the storage_ member; elements are created with placement new");
            return;
        }
    }
    for (std::size_t p : ::zth::find_word(code, "new")) {
        if (p > 0 && code[p - 1] == '<') continue;  // #include <new>
        std::size_t q = p + 3;
        while (q < code.size() && std::isspace(static_cast<unsigned char>(code[q]))) ++q;
        if (q >= code.size() || code[q] != '(') {
            ::zth::fail("static_vector.hpp uses `new T`: use PLACEMENT new, `::new (pointer) T(args)`, into storage_");
            return;
        }
    }
    ZTH_EXPECT_TRUE(true);
}
ZTH_TEST("clean", "static_vector.hpp does not include a standard container: the storage is a byte array in the object") {
    const std::string raw = ::zth::read_file("static_vector.hpp");
    for (const char* header : {"<vector>", "<array>", "<deque>", "<list>", "<forward_list>", "<map>", "<optional>"}) {
        if (raw.find(header) != std::string::npos) {
            ::zth::fail(std::string("static_vector.hpp includes ") + header + ": the point is to manage the elements' lifetimes yourself");
            return;
        }
    }
    ZTH_EXPECT_TRUE(true);
}
ZTH_TEST("clean", "static_vector.hpp has no `using namespace`") {
    const auto layout = ::zth::source_layout_of("static_vector.hpp");
    ZTH_EXPECT_FALSE(layout.using_directive);
}
