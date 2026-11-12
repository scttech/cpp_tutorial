#define ZTH_TRACK_ALLOCATIONS
#include <cstdint>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "static_vector.hpp"
#include "zth_test.hpp"

using emb::StaticVector;

namespace {

// Counts everything and checks that no object is ever copied around byte by byte (it remembers its own address).
struct Tracked {
    int id;
    const Tracked* self;
    static inline int live = 0, ctors = 0, copies = 0, moves = 0, dtors = 0;
    explicit Tracked(int i) : id(i), self(this) {
        ++live;
        ++ctors;
    }
    Tracked(const Tracked& o) : id(o.id), self(this) {
        ++live;
        ++copies;
    }
    Tracked(Tracked&& o) noexcept : id(o.id), self(this) {
        o.id = -1;
        ++live;
        ++moves;
    }
    Tracked& operator=(const Tracked& o) {
        id = o.id;
        ++copies;
        return *this;
    }
    Tracked& operator=(Tracked&& o) noexcept {
        id = o.id;
        o.id = -1;
        ++moves;
        return *this;
    }
    ~Tracked() {
        --live;
        ++dtors;
    }
    bool valid() const { return self == this; }
    static void reset() { ctors = copies = moves = dtors = 0; }
};

template <std::size_t N>
std::string ids(StaticVector<Tracked, N>& v) {
    std::string s;
    for (std::size_t i = 0; i < v.size(); ++i) s += (v[i].valid() ? std::to_string(v[i].id) : std::string("!")) + " ";
    return s;
}

struct alignas(64) Wide {
    int v = 0;
};

}  // namespace

// ================================================ Works =======================================================
ZTH_TEST("works", "an empty vector constructs nothing and knows its capacity") {
    Tracked::reset();
    {
        StaticVector<Tracked, 4> v;
        ZTH_EXPECT_TRUE(v.empty());
        ZTH_EXPECT_FALSE(v.full());
        ZTH_EXPECT_EQ(v.size(), std::size_t{0});
        ZTH_EXPECT_EQ(v.capacity(), std::size_t{4});
        ZTH_EXPECT_TRUE(v.begin() == v.end());
        ZTH_EXPECT_EQ(Tracked::live, 0);
        ZTH_EXPECT_EQ(Tracked::ctors + Tracked::copies + Tracked::moves, 0);   // no element exists for unused slots
    }
    ZTH_EXPECT_EQ(Tracked::dtors, 0);
}
ZTH_TEST("works", "emplace_back builds the element in place: one construction, no copy, no move") {
    Tracked::reset();
    StaticVector<Tracked, 3> v;
    Tracked* p = v.emplace_back(7);
    ZTH_EXPECT_TRUE(p != nullptr);
    ZTH_EXPECT_EQ(Tracked::ctors, 1);
    ZTH_EXPECT_EQ(Tracked::copies + Tracked::moves, 0);
    ZTH_EXPECT_TRUE(p == &v[0]);
    ZTH_EXPECT_EQ(v[0].id, 7);
    ZTH_EXPECT_EQ(v.size(), std::size_t{1});
}
ZTH_TEST("works", "push_back of an rvalue moves once, of an lvalue copies once") {
    Tracked::reset();
    StaticVector<Tracked, 3> v;
    Tracked a(1);
    ZTH_EXPECT_TRUE(v.push_back(a));
    ZTH_EXPECT_EQ(Tracked::copies, 1);
    ZTH_EXPECT_EQ(Tracked::moves, 0);
    ZTH_EXPECT_TRUE(v.push_back(Tracked(2)));
    ZTH_EXPECT_EQ(Tracked::copies, 1);
    ZTH_EXPECT_EQ(Tracked::moves, 1);
    ZTH_EXPECT_EQ(ids(v), std::string("1 2 "));
}
ZTH_TEST("works", "a full vector refuses new elements and constructs nothing") {
    Tracked::reset();
    StaticVector<Tracked, 2> v;
    v.emplace_back(1);
    v.emplace_back(2);
    ZTH_EXPECT_TRUE(v.full());
    const int ctors = Tracked::ctors, moves = Tracked::moves, copies = Tracked::copies;
    ZTH_EXPECT_TRUE(v.emplace_back(3) == nullptr);
    ZTH_EXPECT_FALSE(v.push_back(Tracked(4)));
    Tracked five(5);
    ZTH_EXPECT_FALSE(v.push_back(five));
    ZTH_EXPECT_FALSE(v.insert(0, Tracked(6)));
    ZTH_EXPECT_EQ(v.size(), std::size_t{2});
    ZTH_EXPECT_EQ(ids(v), std::string("1 2 "));
    ZTH_EXPECT_EQ(Tracked::copies, copies);
    ZTH_EXPECT_EQ(Tracked::ctors, ctors + 3);   // only the three temporaries of this test
    (void)moves;
}
ZTH_TEST("works", "pop_back destroys the last element and does nothing on an empty vector") {
    Tracked::reset();
    StaticVector<Tracked, 3> v;
    v.pop_back();
    ZTH_EXPECT_EQ(Tracked::dtors, 0);
    v.emplace_back(1);
    v.emplace_back(2);
    v.pop_back();
    ZTH_EXPECT_EQ(Tracked::dtors, 1);
    ZTH_EXPECT_EQ(Tracked::live, 1);
    ZTH_EXPECT_EQ(v.size(), std::size_t{1});
    ZTH_EXPECT_EQ(v.back().id, 1);
}
ZTH_TEST("works", "indexing, at, front, back, data and iteration") {
    StaticVector<Tracked, 5> v;
    for (int i = 10; i < 14; ++i) v.emplace_back(i);
    ZTH_EXPECT_EQ(v[2].id, 12);
    ZTH_EXPECT_TRUE(v.at(3) != nullptr && v.at(3)->id == 13);
    ZTH_EXPECT_TRUE(v.at(4) == nullptr);
    ZTH_EXPECT_TRUE(v.at(1000) == nullptr);
    ZTH_EXPECT_EQ(v.front().id, 10);
    ZTH_EXPECT_EQ(v.back().id, 13);
    ZTH_EXPECT_TRUE(v.data() == &v[0]);
    int sum = 0;
    for (const Tracked& t : v) sum += t.id;
    ZTH_EXPECT_EQ(sum, 46);
    ZTH_EXPECT_EQ(v.end() - v.begin(), std::ptrdiff_t{4});
    const StaticVector<Tracked, 5>& cv = v;
    ZTH_EXPECT_EQ(cv.end() - cv.begin(), std::ptrdiff_t{4});
    ZTH_EXPECT_EQ(cv[1].id, 11);
}
ZTH_TEST("works", "insert in the middle, at the front and at the end shifts the later elements up") {
    StaticVector<Tracked, 6> v;
    for (int i : {1, 2, 3, 4}) v.emplace_back(i);
    ZTH_EXPECT_TRUE(v.insert(2, Tracked(99)));
    ZTH_EXPECT_EQ(ids(v), std::string("1 2 99 3 4 "));
    ZTH_EXPECT_TRUE(v.insert(0, Tracked(0)));
    ZTH_EXPECT_EQ(ids(v), std::string("0 1 2 99 3 4 "));
    ZTH_EXPECT_TRUE(v.full());
    v.pop_back();
    ZTH_EXPECT_TRUE(v.insert(v.size(), Tracked(7)));
    ZTH_EXPECT_EQ(ids(v), std::string("0 1 2 99 3 7 "));
    ZTH_EXPECT_FALSE(v.insert(9, Tracked(8)));
    StaticVector<Tracked, 3> e;
    ZTH_EXPECT_TRUE(e.insert(0, Tracked(5)));
    ZTH_EXPECT_FALSE(e.insert(2, Tracked(6)));
    ZTH_EXPECT_EQ(ids(e), std::string("5 "));
}
ZTH_TEST("works", "erase shifts the later elements down and destroys exactly one object") {
    Tracked::reset();
    StaticVector<Tracked, 6> v;
    for (int i : {1, 2, 3, 4, 5}) v.emplace_back(i);
    const int live = Tracked::live;
    ZTH_EXPECT_TRUE(v.erase(1));
    ZTH_EXPECT_EQ(ids(v), std::string("1 3 4 5 "));
    ZTH_EXPECT_EQ(Tracked::live, live - 1);
    ZTH_EXPECT_TRUE(v.erase(3));
    ZTH_EXPECT_EQ(ids(v), std::string("1 3 4 "));
    ZTH_EXPECT_TRUE(v.erase(0));
    ZTH_EXPECT_EQ(ids(v), std::string("3 4 "));
    ZTH_EXPECT_FALSE(v.erase(2));
    ZTH_EXPECT_FALSE(v.erase(100));
    ZTH_EXPECT_EQ(v.size(), std::size_t{2});
}
ZTH_TEST("works", "swap_remove replaces the element with the last one") {
    StaticVector<Tracked, 6> v;
    for (int i : {1, 2, 3, 4, 5}) v.emplace_back(i);
    ZTH_EXPECT_TRUE(v.swap_remove(1));
    ZTH_EXPECT_EQ(ids(v), std::string("1 5 3 4 "));
    ZTH_EXPECT_TRUE(v.swap_remove(3));
    ZTH_EXPECT_EQ(ids(v), std::string("1 5 3 "));
    ZTH_EXPECT_FALSE(v.swap_remove(3));
    ZTH_EXPECT_TRUE(v.swap_remove(0));
    ZTH_EXPECT_EQ(ids(v), std::string("3 5 "));
    ZTH_EXPECT_TRUE(v.swap_remove(1));
    ZTH_EXPECT_TRUE(v.swap_remove(0));
    ZTH_EXPECT_TRUE(v.empty());
}
ZTH_TEST("works", "clear and the destructor destroy every element exactly once") {
    Tracked::reset();
    {
        StaticVector<Tracked, 5> v;
        for (int i = 0; i < 5; ++i) v.emplace_back(i);
        v.clear();
        ZTH_EXPECT_EQ(Tracked::dtors, 5);
        ZTH_EXPECT_EQ(Tracked::live, 0);
        ZTH_EXPECT_TRUE(v.empty());
        v.emplace_back(1);
        v.emplace_back(2);
    }
    ZTH_EXPECT_EQ(Tracked::dtors, 7);
    ZTH_EXPECT_EQ(Tracked::live, 0);
}
ZTH_TEST("works", "copying makes independent copies of the elements (and builds them, not memcpy's them)") {
    Tracked::reset();
    StaticVector<Tracked, 4> a;
    for (int i : {1, 2, 3}) a.emplace_back(i);
    StaticVector<Tracked, 4> b(a);
    ZTH_EXPECT_EQ(Tracked::copies, 3);
    ZTH_EXPECT_EQ(ids(b), std::string("1 2 3 "));
    b[0].id = 50;
    ZTH_EXPECT_EQ(ids(a), std::string("1 2 3 "));
    ZTH_EXPECT_EQ(Tracked::live, 6);
}
ZTH_TEST("works", "copy assignment: grows, shrinks, empties and survives self-assignment, with no object leaked") {
    Tracked::reset();
    StaticVector<Tracked, 5> a, b;
    for (int i : {1, 2, 3}) a.emplace_back(i);
    for (int i : {7, 8, 9, 10, 11}) b.emplace_back(i);
    b = a;   // shrink
    ZTH_EXPECT_EQ(ids(b), std::string("1 2 3 "));
    ZTH_EXPECT_EQ(Tracked::live, 6);
    StaticVector<Tracked, 5> c;
    c = a;   // grow from empty
    ZTH_EXPECT_EQ(ids(c), std::string("1 2 3 "));
    StaticVector<Tracked, 5> empty;
    c = empty;
    ZTH_EXPECT_TRUE(c.empty());
    ZTH_EXPECT_EQ(Tracked::live, 6);
    StaticVector<Tracked, 5>& alias = a;
    a = alias;
    ZTH_EXPECT_EQ(ids(a), std::string("1 2 3 "));
    ZTH_EXPECT_EQ(Tracked::live, 6);
}
ZTH_TEST("works", "moving moves the elements one by one and leaves the source empty") {
    Tracked::reset();
    StaticVector<Tracked, 4> a;
    for (int i : {1, 2, 3}) a.emplace_back(i);
    StaticVector<Tracked, 4> b(std::move(a));
    ZTH_EXPECT_EQ(Tracked::moves, 3);
    ZTH_EXPECT_EQ(Tracked::copies, 0);
    ZTH_EXPECT_EQ(ids(b), std::string("1 2 3 "));
    ZTH_EXPECT_TRUE(a.empty());
    ZTH_EXPECT_EQ(Tracked::live, 3);
    StaticVector<Tracked, 4> c;
    c.emplace_back(40);
    c = std::move(b);
    ZTH_EXPECT_EQ(ids(c), std::string("1 2 3 "));
    ZTH_EXPECT_TRUE(b.empty());
    ZTH_EXPECT_EQ(Tracked::copies, 0);
    ZTH_EXPECT_EQ(Tracked::live, 3);
    StaticVector<Tracked, 4>& same = c;
    c = std::move(same);
    ZTH_EXPECT_EQ(ids(c), std::string("1 2 3 "));
}
ZTH_TEST("works", "move-only elements work (a unique_ptr vector)") {
    StaticVector<std::unique_ptr<int>, 4> v;
    ZTH_EXPECT_TRUE(v.push_back(std::make_unique<int>(1)));
    ZTH_EXPECT_TRUE(v.emplace_back(new int(2)) != nullptr);
    ZTH_EXPECT_TRUE(v.insert(1, std::make_unique<int>(3)));
    ZTH_EXPECT_EQ(*v[0], 1);
    ZTH_EXPECT_EQ(*v[1], 3);
    ZTH_EXPECT_EQ(*v[2], 2);
    ZTH_EXPECT_TRUE(v.erase(0));
    ZTH_EXPECT_EQ(*v[0], 3);
    StaticVector<std::unique_ptr<int>, 4> w(std::move(v));
    ZTH_EXPECT_EQ(w.size(), std::size_t{2});
    ZTH_EXPECT_EQ(*w[1], 2);
    ZTH_EXPECT_TRUE(v.empty());
}
ZTH_TEST("works", "an element type without a default constructor, and one with a heap string") {
    StaticVector<Tracked, 2> v;   // Tracked has no default constructor: unused slots must not be constructed
    v.emplace_back(1);
    StaticVector<std::string, 3> s;
    s.push_back(std::string("a string that is long enough to need the heap, not the small buffer"));
    s.emplace_back(5, 'x');
    StaticVector<std::string, 3> t(s);
    ZTH_EXPECT_EQ(t[0], s[0]);
    ZTH_EXPECT_EQ(t[1], std::string("xxxxx"));
    s.clear();
    ZTH_EXPECT_EQ(t[0].size(), std::size_t{67});
}
ZTH_TEST("works", "storage is aligned for the element type") {
    StaticVector<Wide, 2> vs[6];
    for (auto& v : vs) {
        v.emplace_back();
        ZTH_EXPECT_EQ(reinterpret_cast<std::uintptr_t>(&v[0]) % 64, std::uintptr_t{0});
    }
    StaticVector<double, 3> d;
    d.push_back(1.5);
    ZTH_EXPECT_EQ(reinterpret_cast<std::uintptr_t>(&d[0]) % alignof(double), std::uintptr_t{0});
}

// ================================================ Robust ======================================================
ZTH_TEST("robust", "20000 random operations against std::vector: contents, size, live objects, and no byte-copied element") {
    zth::Lcg rng(31);
    Tracked::reset();
    {
        StaticVector<Tracked, 12> v;
        std::vector<int> ref;
        for (int step = 0; step < 20000; ++step) {
            const int op = rng.below(100);
            const int value = rng.below(1000);
            if (op < 28) {
                const bool ok = v.push_back(Tracked(value));
                if (ok != (ref.size() < 12)) { zth::fail("push_back result wrong at step " + std::to_string(step)); return; }
                if (ok) ref.push_back(value);
            } else if (op < 40) {
                v.pop_back();
                if (!ref.empty()) ref.pop_back();
            } else if (op < 58) {
                const std::size_t i = static_cast<std::size_t>(rng.below(14));
                const bool ok = v.insert(i, Tracked(value));
                const bool expect = ref.size() < 12 && i <= ref.size();
                if (ok != expect) { zth::fail("insert result wrong at step " + std::to_string(step)); return; }
                if (ok) ref.insert(ref.begin() + static_cast<std::ptrdiff_t>(i), value);
            } else if (op < 76) {
                const std::size_t i = static_cast<std::size_t>(rng.below(14));
                const bool ok = v.erase(i);
                if (ok != (i < ref.size())) { zth::fail("erase result wrong at step " + std::to_string(step)); return; }
                if (ok) ref.erase(ref.begin() + static_cast<std::ptrdiff_t>(i));
            } else if (op < 86) {
                const std::size_t i = static_cast<std::size_t>(rng.below(14));
                const bool ok = v.swap_remove(i);
                if (ok != (i < ref.size())) { zth::fail("swap_remove result wrong at step " + std::to_string(step)); return; }
                if (ok) {
                    ref[i] = ref.back();
                    ref.pop_back();
                }
            } else if (op < 92) {
                StaticVector<Tracked, 12> copy(v);
                v.clear();
                v = std::move(copy);
            } else if (op < 94) {
                v.clear();
                ref.clear();
            } else {
                StaticVector<Tracked, 12> other;
                other = v;
                v = other;
            }
            if (v.size() != ref.size() || Tracked::live != static_cast<int>(ref.size())) {
                zth::fail("size " + std::to_string(v.size()) + " / live objects " + std::to_string(Tracked::live) + " instead of " + std::to_string(ref.size()) + " at step " + std::to_string(step));
                return;
            }
            for (std::size_t i = 0; i < ref.size(); ++i) {
                if (!v[i].valid() || v[i].id != ref[i]) { zth::fail("element " + std::to_string(i) + " is wrong at step " + std::to_string(step)); return; }
            }
        }
    }
    ZTH_EXPECT_EQ(Tracked::live, 0);
}
ZTH_TEST("robust", "elements are never moved around by raw bytes: after any number of operations they still know their own address") {
    StaticVector<Tracked, 8> v;
    for (int i = 0; i < 8; ++i) v.emplace_back(i);
    v.erase(2);
    v.insert(1, Tracked(40));
    v.swap_remove(0);
    StaticVector<Tracked, 8> w(v);
    StaticVector<Tracked, 8> x(std::move(w));
    for (auto& t : v) ZTH_EXPECT_TRUE(t.valid());
    for (auto& t : x) ZTH_EXPECT_TRUE(t.valid());
}
ZTH_TEST("robust", "the vector can hold other vectors") {
    StaticVector<StaticVector<std::string, 2>, 3> nested;
    nested.emplace_back();
    nested[0].push_back(std::string("one"));
    nested.emplace_back(nested[0]);
    nested[1].push_back(std::string("two"));
    ZTH_EXPECT_EQ(nested[0].size(), std::size_t{1});
    ZTH_EXPECT_EQ(nested[1].size(), std::size_t{2});
    auto copy = nested;
    ZTH_EXPECT_EQ(copy[1][1], std::string("two"));
}

// ================================================ Fast ========================================================
ZTH_TEST("fast", "no heap: pushing, copying, moving and erasing allocate nothing (for a trivial element type)") {
    zth::mem::Scope scope;
    StaticVector<int, 64> v;
    for (int i = 0; i < 64; ++i) v.push_back(i);
    StaticVector<int, 64> w(v);
    StaticVector<int, 64> x(std::move(w));
    for (int i = 0; i < 20; ++i) x.erase(3);
    x.insert(0, 5);
    x.swap_remove(1);
    ZTH_EXPECT_ALLOCS_AT_MOST(scope, 0);
    ZTH_EXPECT_TRUE(x.size() > 40);
}
ZTH_TEST("fast", "the object is as big as its storage: N elements plus one counter") {
    ZTH_EXPECT_TRUE(sizeof(StaticVector<char, 10>) <= 10 + 8 + 8);
    ZTH_EXPECT_TRUE(sizeof(StaticVector<std::uint32_t, 100>) <= 400 + 16);
    ZTH_EXPECT_TRUE(sizeof(StaticVector<Tracked, 4>) <= 4 * sizeof(Tracked) + 16);
}
ZTH_TEST("fast", "erase and insert shift with MOVES, one per displaced element, and never copy") {
    Tracked::reset();
    StaticVector<Tracked, 40> v;
    for (int i = 0; i < 30; ++i) v.emplace_back(i);
    Tracked::reset();
    v.erase(0);   // 29 displaced
    ZTH_EXPECT_EQ(Tracked::copies, 0);
    ZTH_EXPECT_EQ(Tracked::moves, 29);
    Tracked::reset();
    v.insert(0, Tracked(100));   // 29 displaced + the value: moves into the parameter, into the slot
    ZTH_EXPECT_EQ(Tracked::copies, 0);
    ZTH_EXPECT_TRUE(Tracked::moves <= 29 + 2);
    ZTH_EXPECT_TRUE(Tracked::moves >= 29);
}
ZTH_TEST("fast", "swap_remove is O(1): at most one move whatever the size") {
    StaticVector<Tracked, 100> v;
    for (int i = 0; i < 100; ++i) v.emplace_back(i);
    Tracked::reset();
    v.swap_remove(0);
    ZTH_EXPECT_TRUE(Tracked::moves <= 1);
    ZTH_EXPECT_EQ(Tracked::copies, 0);
    ZTH_EXPECT_EQ(v[0].id, 99);
}
ZTH_TEST("fast", "building the vector constructs exactly the elements that are stored") {
    Tracked::reset();
    {
        StaticVector<Tracked, 1000> v;   // a thousand slots of raw memory and not one construction
        for (int i = 0; i < 10; ++i) v.emplace_back(i);
        ZTH_EXPECT_EQ(Tracked::ctors, 10);
        ZTH_EXPECT_EQ(Tracked::live, 10);
    }
    ZTH_EXPECT_EQ(Tracked::dtors, 10);
}

// ================================================ Clean =======================================================
ZTH_TEST("clean", "no heap, no exceptions, no goto") {
    const std::string code = ::zth::strip_comments_and_literals(::zth::read_file("static_vector.hpp"));
    for (const char* word : {"new", "delete", "malloc", "calloc", "realloc", "free", "aligned_alloc", "make_unique", "make_shared", "unique_ptr", "shared_ptr"}) ZTH_EXPECT_TRUE(::zth::find_word(code, word).empty());
    for (const char* word : {"throw", "try", "catch", "goto"}) ZTH_EXPECT_TRUE(::zth::find_word(code, word).empty());
}
ZTH_TEST("clean", "objects are built and ended properly: construct_at / destroy_at, no memcpy and no standard containers") {
    const std::string code = ::zth::strip_comments_and_literals(::zth::read_file("static_vector.hpp"));
    ZTH_EXPECT_FALSE(::zth::find_word(code, "construct_at").empty());
    ZTH_EXPECT_FALSE(::zth::find_word(code, "destroy_at").empty());
    for (const char* word : {"memcpy", "memmove", "memset", "copy", "copy_n", "vector", "array", "string", "deque", "list", "optional", "variant"}) ZTH_EXPECT_TRUE(::zth::find_word(code, word).empty());
}
ZTH_TEST("clean", "the storage is aligned with alignas") {
    const std::string code = ::zth::strip_comments_and_literals(::zth::read_file("static_vector.hpp"));
    ZTH_EXPECT_FALSE(::zth::find_word(code, "alignas").empty());
}
