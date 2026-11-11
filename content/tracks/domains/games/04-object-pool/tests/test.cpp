#define ZTH_TRACK_ALLOCATIONS
#include <algorithm>
#include <cstdint>
#include <memory>
#include <set>
#include <string>
#include <type_traits>
#include <vector>

#include "pool.hpp"
#include "zth_test.hpp"

using namespace pool;

namespace {

struct MoveOnly {
    std::unique_ptr<int> p;
    int tag;
    MoveOnly(std::unique_ptr<int> v, int t) : p(std::move(v)), tag(t) {}
};

#ifdef __cpp_exceptions
struct Thrower {
    explicit Thrower(bool boom) : value(7) {
        if (boom) throw 1;
        ++made;
    }
    ~Thrower() { ++unmade; }
    int value;
    static inline int made = 0, unmade = 0;
};
#endif

}  // namespace

// ================================================ Works =======================================================
ZTH_TEST("works", "create, get and destroy; the arguments are forwarded to the constructor") {
    stats::reset();
    auto p = std::make_unique<Pool<Tracked, 4>>();
    const Handle a = p->create(1, 2);
    const Handle b = p->create(5);
    const Handle c = p->create();
    ZTH_EXPECT_TRUE(a.index != b.index && b.index != c.index && a.index != c.index);
    ZTH_EXPECT_EQ(p->get(a)->a, 1);
    ZTH_EXPECT_EQ(p->get(a)->b, 2);
    ZTH_EXPECT_EQ(p->get(b)->a, 5);
    ZTH_EXPECT_EQ(p->get(c)->a, 0);
    ZTH_EXPECT_EQ(p->size(), std::size_t{3});
    ZTH_EXPECT_EQ((Pool<Tracked, 4>::capacity()), std::size_t{4});
    p->get(a)->a = 42;
    ZTH_EXPECT_EQ(p->get(a)->a, 42);
    ZTH_EXPECT_TRUE(p->destroy(b));
    ZTH_EXPECT_EQ(p->size(), std::size_t{2});
    ZTH_EXPECT_TRUE(p->get(b) == nullptr);
    ZTH_EXPECT_TRUE(p->alive(a));
    ZTH_EXPECT_FALSE(p->alive(b));
}
ZTH_TEST("works", "the objects are constructed in place: no copy and no move") {
    stats::reset();
    {
        Pool<Tracked, 8> p;
        for (int i = 0; i < 8; ++i) p.create(i, i);
    }
    ZTH_EXPECT_EQ(stats::copies, 0ULL);
    ZTH_EXPECT_EQ(stats::moves, 0ULL);
    ZTH_EXPECT_EQ(stats::constructed, 8ULL);
}
ZTH_TEST("works", "a full pool refuses with the invalid handle and constructs nothing; destroying makes room again") {
    stats::reset();
    Pool<Tracked, 3> p;
    Handle h[3];
    for (int i = 0; i < 3; ++i) h[i] = p.create(i);
    ZTH_EXPECT_TRUE(p.full());
    const auto before = stats::constructed;
    ZTH_EXPECT_TRUE(p.create(9) == kInvalidHandle);
    ZTH_EXPECT_EQ(stats::constructed, before);
    ZTH_EXPECT_EQ(p.size(), std::size_t{3});
    ZTH_EXPECT_TRUE(p.destroy(h[1]));
    ZTH_EXPECT_FALSE(p.full());
    const Handle again = p.create(77);
    ZTH_EXPECT_FALSE(again == kInvalidHandle);
    ZTH_EXPECT_EQ(p.get(again)->a, 77);
    ZTH_EXPECT_TRUE(p.full());
}
ZTH_TEST("works", "the slot freed last is reused first, with a higher generation") {
    Pool<Tracked, 5> p;
    Handle h[5];
    for (int i = 0; i < 5; ++i) h[i] = p.create(i);
    p.destroy(h[1]);
    p.destroy(h[3]);
    const Handle x = p.create(100);
    ZTH_EXPECT_EQ(x.index, h[3].index);
    ZTH_EXPECT_EQ(x.generation, h[3].generation + 1);
    const Handle y = p.create(200);
    ZTH_EXPECT_EQ(y.index, h[1].index);
    ZTH_EXPECT_EQ(y.generation, h[1].generation + 1);
}
ZTH_TEST("works", "a stale handle is refused even when its slot holds a new object") {
    Pool<Tracked, 2> p;
    const Handle old = p.create(1);
    p.destroy(old);
    const Handle fresh = p.create(2);
    ZTH_EXPECT_EQ(fresh.index, old.index);
    ZTH_EXPECT_TRUE(p.get(old) == nullptr);
    ZTH_EXPECT_FALSE(p.alive(old));
    ZTH_EXPECT_FALSE(p.destroy(old));  // must not destroy the new object
    ZTH_EXPECT_TRUE(p.alive(fresh));
    ZTH_EXPECT_EQ(p.get(fresh)->a, 2);
    ZTH_EXPECT_EQ(p.size(), std::size_t{1});
}
ZTH_TEST("works", "destructors run once on destroy and for the remaining objects when the pool goes away") {
    stats::reset();
    {
        Pool<Tracked, 6> p;
        Handle h[6];
        for (int i = 0; i < 6; ++i) h[i] = p.create(i);
        p.destroy(h[0]);
        p.destroy(h[4]);
        ZTH_EXPECT_EQ(stats::live(), 4LL);
    }
    ZTH_EXPECT_EQ(stats::live(), 0LL);
    ZTH_EXPECT_EQ(stats::destroyed, 6ULL);
}
ZTH_TEST("works", "for_each visits the live objects, in slot order, with their handles") {
    Pool<Tracked, 6> p;
    for (int round = 0; round < 3; ++round) {  // so that the slots have generations above 0
        Handle t[6];
        for (int i = 0; i < 6; ++i) t[i] = p.create(i);
        for (int i = 0; i < 6; ++i) p.destroy(t[i]);
    }
    Handle h[6];
    for (int i = 0; i < 6; ++i) h[i] = p.create(i * 10);
    p.destroy(h[1]);
    p.destroy(h[4]);
    std::vector<int> seen;
    std::vector<std::uint32_t> slots;
    p.for_each([&](Handle handle, Tracked& t) {
        seen.push_back(t.a);
        slots.push_back(handle.index);
        ZTH_EXPECT_TRUE(p.get(handle) == &t);
        t.b = 1;
    });
    ZTH_EXPECT_EQ(slots.size(), std::size_t{4});
    for (std::size_t k = 1; k < slots.size(); ++k) ZTH_EXPECT_TRUE(slots[k - 1] < slots[k]);   // in slot order
    std::sort(seen.begin(), seen.end());
    ZTH_EXPECT_TRUE((seen == std::vector<int>{0, 20, 30, 50}));   // exactly the live objects
    ZTH_EXPECT_EQ(p.get(h[2])->b, 1);
}
ZTH_TEST("works", "pointers to objects stay valid while other objects come and go") {
    Pool<Tracked, 10> p;
    const Handle a = p.create(11);
    Tracked* where = p.get(a);
    for (int round = 0; round < 20; ++round) {
        const Handle t = p.create(round);
        p.destroy(t);
    }
    ZTH_EXPECT_TRUE(p.get(a) == where);
    ZTH_EXPECT_EQ(where->a, 11);
}

// ================================================ Robust ======================================================
ZTH_TEST("robust", "destroy of the invalid handle, of an out-of-range index, and twice in a row is refused without harm") {
    stats::reset();
    Pool<Tracked, 4> p;
    const Handle a = p.create(1);
    ZTH_EXPECT_FALSE(p.destroy(kInvalidHandle));
    ZTH_EXPECT_FALSE(p.destroy(Handle{4, 0}));
    ZTH_EXPECT_FALSE(p.destroy(Handle{1000, 0}));
    ZTH_EXPECT_FALSE(p.destroy(Handle{2, 0}));  // a free slot, never used
    ZTH_EXPECT_TRUE(p.get(Handle{4, 0}) == nullptr);
    ZTH_EXPECT_TRUE(p.get(kInvalidHandle) == nullptr);
    ZTH_EXPECT_FALSE(p.alive(Handle{0xFFFFFFFFu, 0}));
    ZTH_EXPECT_TRUE(p.destroy(a));
    ZTH_EXPECT_FALSE(p.destroy(a));
    ZTH_EXPECT_FALSE(p.destroy(a));
    ZTH_EXPECT_EQ(stats::destroyed, 1ULL);
    ZTH_EXPECT_EQ(p.size(), std::size_t{0});
    // the free list is intact after all that: all four slots can be used, and no more
    for (int i = 0; i < 4; ++i) ZTH_EXPECT_FALSE(p.create(i) == kInvalidHandle);
    ZTH_EXPECT_TRUE(p.create(9) == kInvalidHandle);
}
ZTH_TEST("robust", "a pool of one, and a pool that is used up and emptied many times") {
    Pool<Tracked, 1> one;
    for (int i = 0; i < 100; ++i) {
        const Handle h = one.create(i);
        ZTH_EXPECT_FALSE(h == kInvalidHandle);
        ZTH_EXPECT_TRUE(one.create(0) == kInvalidHandle);
        ZTH_EXPECT_EQ(one.get(h)->a, i);
        ZTH_EXPECT_EQ(h.generation, static_cast<std::uint32_t>(i));
        ZTH_EXPECT_TRUE(one.destroy(h));
    }
}
ZTH_TEST("robust", "storage is aligned for over-aligned types, in every slot, wherever the pool itself lives") {
    std::vector<std::unique_ptr<Pool<Wide, 5>>> pools;
    for (int k = 0; k < 12; ++k) {
        pools.push_back(std::make_unique<Pool<Wide, 5>>());
        std::vector<char> spacer(static_cast<std::size_t>(8 * k + 3));   // moves the next pool to another address
        (void)spacer;
        auto& p = pools.back();
        for (int i = 0; i < 5; ++i) {
            const Handle h = p->create();
            ZTH_EXPECT_EQ(reinterpret_cast<std::uintptr_t>(p->get(h)) % 32, std::uintptr_t{0});
            ZTH_EXPECT_EQ(p->get(h)->v[2], 3.0);
        }
    }
    Pool<char, 7> chars;  // and small types work too
    const Handle c = chars.create('x');
    ZTH_EXPECT_EQ(*chars.get(c), 'x');
}
ZTH_TEST("robust", "move-only arguments are forwarded, not copied") {
    Pool<MoveOnly, 3> p;
    auto value = std::make_unique<int>(41);
    int* raw = value.get();
    const Handle h = p.create(std::move(value), 5);
    ZTH_EXPECT_TRUE(value == nullptr);
    ZTH_EXPECT_TRUE(p.get(h)->p.get() == raw);
    ZTH_EXPECT_EQ(p.get(h)->tag, 5);
}
ZTH_TEST("robust", "const access: get on a const pool, alive on a const pool") {
    Pool<Tracked, 3> p;
    const Handle h = p.create(8);
    const Pool<Tracked, 3>& cp = p;
    ZTH_EXPECT_TRUE(cp.alive(h));
    ZTH_EXPECT_EQ(cp.get(h)->a, 8);
    ZTH_EXPECT_EQ(cp.size(), std::size_t{1});
    ZTH_EXPECT_FALSE(cp.full());
}
ZTH_TEST("robust", "the pool cannot be copied or assigned") {
    ZTH_EXPECT_FALSE((std::is_copy_constructible_v<Pool<Tracked, 3>>));
    ZTH_EXPECT_FALSE((std::is_copy_assignable_v<Pool<Tracked, 3>>));
}
#ifdef __cpp_exceptions
ZTH_TEST("robust", "a constructor that throws leaves the pool unchanged: no slot is lost") {
    Thrower::made = Thrower::unmade = 0;
    Pool<Thrower, 3> p;
    for (int i = 0; i < 5; ++i) {
        bool threw = false;
        try {
            p.create(true);
        } catch (int) {
            threw = true;
        }
        ZTH_EXPECT_TRUE(threw);
    }
    ZTH_EXPECT_EQ(p.size(), std::size_t{0});
    Handle h[3];
    for (int i = 0; i < 3; ++i) {
        h[i] = p.create(false);
        ZTH_EXPECT_FALSE(h[i] == kInvalidHandle);
    }
    ZTH_EXPECT_TRUE(p.full());
    ZTH_EXPECT_EQ(p.get(h[0])->value, 7);
}
#endif
ZTH_TEST_TIMEOUT("robust", "20000 random operations on a pool of 50 agree with a model; objects alive = pool size at every step", 10) {
    stats::reset();
    ::zth::Lcg rng(55);
    Pool<Tracked, 50> p;
    struct Entry {
        Handle h;
        int value;
        bool alive;
    };
    std::vector<Entry> all;
    std::size_t live = 0;
    for (int step = 0; step < 20000; ++step) {
        if (rng.below(2) == 0) {
            const Handle h = p.create(step);
            if (live < 50) {
                ZTH_EXPECT_FALSE(h == kInvalidHandle);
                all.push_back({h, step, true});
                ++live;
            } else {
                ZTH_EXPECT_TRUE(h == kInvalidHandle);
            }
        } else if (!all.empty()) {
            Entry& e = all[static_cast<std::size_t>(rng.below(static_cast<int>(all.size())))];
            ZTH_EXPECT_EQ(p.destroy(e.h), e.alive);
            if (e.alive) {
                e.alive = false;
                --live;
            }
        }
        ZTH_EXPECT_EQ(p.size(), live);
        ZTH_EXPECT_EQ(stats::live(), static_cast<long long>(live));
        if (step % 100 == 0) {
            for (const Entry& e : all) {
                ZTH_EXPECT_EQ(p.alive(e.h), e.alive);
                if (e.alive) ZTH_EXPECT_EQ(p.get(e.h)->a, e.value);
            }
        }
    }
}

// ================================================ Fast ========================================================
ZTH_TEST_TIMEOUT("fast", "create and destroy take constant time: 2 million cycles on a pool of 200000 (a search for a free slot would take hours)", 10) {
    auto p = std::make_unique<Pool<std::uint64_t, 200000>>();
    std::vector<Handle> hs(200000);
    for (std::size_t i = 0; i < hs.size(); ++i) hs[i] = p->create(i);
    ZTH_EXPECT_TRUE(p->full());
    ::zth::Lcg rng(1);
    for (int i = 0; i < 2000000; ++i) {
        Handle& h = hs[static_cast<std::size_t>(rng.below(200000))];
        ZTH_EXPECT_TRUE(p->destroy(h));
        h = p->create(static_cast<std::uint64_t>(i));
        if (h == kInvalidHandle) {
            ::zth::fail("create failed although a slot was just freed");
            return;
        }
    }
}
ZTH_TEST_TIMEOUT("fast", "the pool never touches the heap: not one allocation in 1000000 operations", 10) {
    auto p = std::make_unique<Pool<Tracked, 1000>>();
    zth::mem::Scope scope;
    Handle hs[1000];
    for (int round = 0; round < 500; ++round) {
        for (int i = 0; i < 1000; ++i) hs[i] = p->create(i, round);
        for (int i = 0; i < 1000; ++i) p->destroy(hs[i]);
    }
    ZTH_EXPECT_ALLOCS_AT_MOST(scope, 0);
}
ZTH_TEST_TIMEOUT("fast", "the bookkeeping is small: at most 16 bytes per slot on top of the objects", 10) {
    ZTH_EXPECT_TRUE(sizeof(Pool<std::uint64_t, 1000>) <= 1000 * (sizeof(std::uint64_t) + 16) + 64);
    ZTH_EXPECT_TRUE(sizeof(Pool<std::uint8_t, 1000>) <= 1000 * (1 + 16) + 64);
    ZTH_EXPECT_TRUE(sizeof(Pool<Tracked, 100>) <= 100 * (sizeof(Tracked) + 16) + 64);
}
ZTH_TEST_TIMEOUT("fast", "objects are built in place and destroyed exactly once (a million creates: no copy, no move)", 10) {
    stats::reset();
    {
        auto p = std::make_unique<Pool<Tracked, 1000>>();
        Handle hs[1000];
        for (int round = 0; round < 1000; ++round) {
            for (int i = 0; i < 1000; ++i) hs[i] = p->create(i);
            for (int i = 0; i < 1000; i += 2) p->destroy(hs[i]);
            for (int i = 0; i < 1000; i += 2) hs[i] = p->create(i);
            for (int i = 0; i < 1000; ++i) p->destroy(hs[i]);
        }
    }
    ZTH_EXPECT_EQ(stats::copies + stats::moves, 0ULL);
    ZTH_EXPECT_EQ(stats::constructed, stats::destroyed);
}

// ================================================ Clean =======================================================
ZTH_TEST("clean", "no raw new/delete/malloc/free (construct_at and destroy_at do the construction)") {
    ZTH_EXPECT_NO_RAW_MEMORY("pool.hpp");
    const std::string code = ::zth::strip_comments_and_literals(::zth::read_file("pool.hpp"));
    ZTH_EXPECT_FALSE(::zth::find_word(code, "construct_at").empty());
    ZTH_EXPECT_FALSE(::zth::find_word(code, "destroy_at").empty());
}
ZTH_TEST("clean", "the pool owns its storage: no std::vector, std::unique_ptr, std::function, no exceptions handling, no goto") {
    const std::string code = ::zth::strip_comments_and_literals(::zth::read_file("pool.hpp"));
    for (const char* word : {"vector", "unique_ptr", "shared_ptr", "make_unique", "function", "list", "deque", "map", "try", "catch", "goto", "optional"}) {
        if (!::zth::find_word(code, word).empty()) {
            ::zth::fail(std::string("pool.hpp uses `") + word + "`");
            return;
        }
    }
}
ZTH_TEST("clean", "arguments are perfectly forwarded: std::forward is used") {
    const std::string code = ::zth::strip_comments_and_literals(::zth::read_file("pool.hpp"));
    ZTH_EXPECT_FALSE(::zth::find_word(code, "forward").empty());
}
