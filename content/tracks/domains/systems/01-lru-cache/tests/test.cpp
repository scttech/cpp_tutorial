#define ZTH_TRACK_ALLOCATIONS
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "lru.hpp"
#include "zth_test.hpp"

using lru::LruCache;

namespace {

// A key that counts what the cache does with it.
struct Key {
    int id;
    static inline long copies = 0, eqs = 0, hashes = 0;
    Key(int i = 0) : id(i) {}
    Key(const Key& o) : id(o.id) { ++copies; }
    Key(Key&& o) noexcept : id(o.id) {}
    Key& operator=(const Key& o) {
        id = o.id;
        ++copies;
        return *this;
    }
    Key& operator=(Key&&) noexcept = default;
    bool operator==(const Key& o) const {
        ++eqs;
        return id == o.id;
    }
    static void reset() { copies = eqs = hashes = 0; }
};
struct KeyHash {
    std::size_t operator()(const Key& k) const {
        ++Key::hashes;
        return std::hash<int>()(k.id);
    }
};
// A value that counts copies.
struct Val {
    int v;
    static inline long copies = 0;
    Val(int x = 0) : v(x) {}
    Val(const Val& o) : v(o.v) { ++copies; }
    Val(Val&&) noexcept = default;
    Val& operator=(const Val& o) {
        v = o.v;
        ++copies;
        return *this;
    }
    Val& operator=(Val&&) noexcept = default;
};

using Cache = LruCache<int, int>;

std::vector<int> order(const Cache& c) {
    std::vector<int> keys;
    c.for_each([&](int k, int) { keys.push_back(k); });
    return keys;
}
std::string show(const std::vector<int>& v) {
    std::string s;
    for (int x : v) s += std::to_string(x) + " ";
    return s;
}

// The obvious slow reference: a vector, front = most recent.
struct Reference {
    std::size_t cap;
    std::vector<std::pair<int, int>> e;
    explicit Reference(std::size_t c) : cap(c) {}
    int find(int k) const {
        for (std::size_t i = 0; i < e.size(); ++i)
            if (e[i].first == k) return static_cast<int>(i);
        return -1;
    }
    void promote(int i) {
        auto item = e[static_cast<std::size_t>(i)];
        e.erase(e.begin() + i);
        e.insert(e.begin(), item);
    }
    bool put(int k, int v) {
        if (cap == 0) return false;
        const int i = find(k);
        if (i >= 0) {
            e[static_cast<std::size_t>(i)].second = v;
            promote(i);
            return false;
        }
        bool evicted = false;
        if (e.size() == cap) {
            e.pop_back();
            evicted = true;
        }
        e.insert(e.begin(), {k, v});
        return evicted;
    }
};

}  // namespace

// ================================================ Works =======================================================
ZTH_TEST("works", "an empty cache") {
    Cache c(3);
    ZTH_EXPECT_TRUE(c.empty());
    ZTH_EXPECT_EQ(c.size(), std::size_t{0});
    ZTH_EXPECT_EQ(c.capacity(), std::size_t{3});
    ZTH_EXPECT_TRUE(c.get(1) == nullptr);
    ZTH_EXPECT_FALSE(c.contains(1));
    ZTH_EXPECT_TRUE(c.peek(1) == nullptr);
    ZTH_EXPECT_FALSE(c.erase(1));
}
ZTH_TEST("works", "put then get returns the value, and the pointer can modify it") {
    Cache c(3);
    ZTH_EXPECT_FALSE(c.put(1, 10));
    ZTH_EXPECT_FALSE(c.put(2, 20));
    int* p = c.get(1);
    ZTH_EXPECT_TRUE(p != nullptr);
    if (p == nullptr) return;
    ZTH_EXPECT_EQ(*p, 10);
    *p = 11;
    ZTH_EXPECT_EQ(*c.get(1), 11);
    ZTH_EXPECT_EQ(*c.get(2), 20);
    ZTH_EXPECT_EQ(c.size(), std::size_t{2});
    ZTH_EXPECT_FALSE(c.empty());
}
ZTH_TEST("works", "when full, put evicts the least recently used entry and says so") {
    Cache c(3);
    ZTH_EXPECT_FALSE(c.put(1, 1));
    ZTH_EXPECT_FALSE(c.put(2, 2));
    ZTH_EXPECT_FALSE(c.put(3, 3));
    ZTH_EXPECT_TRUE(c.put(4, 4));
    ZTH_EXPECT_FALSE(c.contains(1));
    ZTH_EXPECT_TRUE(c.contains(2));
    ZTH_EXPECT_TRUE(c.contains(3));
    ZTH_EXPECT_TRUE(c.contains(4));
    ZTH_EXPECT_EQ(c.size(), std::size_t{3});
    ZTH_EXPECT_TRUE(c.get(1) == nullptr);
}
ZTH_TEST("works", "get makes an entry the most recently used") {
    Cache c(3);
    c.put(1, 1);
    c.put(2, 2);
    c.put(3, 3);
    ZTH_EXPECT_TRUE(c.get(1) != nullptr);  // 1 is now the freshest; 2 is the oldest
    ZTH_EXPECT_TRUE(c.put(4, 4));
    ZTH_EXPECT_TRUE(c.contains(1));
    ZTH_EXPECT_FALSE(c.contains(2));
    ZTH_EXPECT_EQ(show(order(c)), std::string("4 1 3 "));
}
ZTH_TEST("works", "put on an existing key updates the value, promotes it and never evicts") {
    Cache c(2);
    c.put(1, 1);
    c.put(2, 2);
    ZTH_EXPECT_FALSE(c.put(1, 100));  // full, but 1 already exists: no eviction
    ZTH_EXPECT_EQ(c.size(), std::size_t{2});
    ZTH_EXPECT_EQ(*c.peek(1), 100);
    ZTH_EXPECT_TRUE(c.contains(2));
    ZTH_EXPECT_TRUE(c.put(3, 3));  // 2 is the oldest now
    ZTH_EXPECT_FALSE(c.contains(2));
    ZTH_EXPECT_EQ(show(order(c)), std::string("3 1 "));
}
ZTH_TEST("works", "peek and contains look without touching: no promotion, no hit counted") {
    Cache c(2);
    c.put(1, 1);
    c.put(2, 2);
    ZTH_EXPECT_EQ(*c.peek(1), 1);
    ZTH_EXPECT_TRUE(c.contains(1));
    ZTH_EXPECT_TRUE(c.peek(9) == nullptr);
    ZTH_EXPECT_FALSE(c.contains(9));
    ZTH_EXPECT_EQ(c.hits(), std::size_t{0});
    ZTH_EXPECT_EQ(c.misses(), std::size_t{0});
    ZTH_EXPECT_TRUE(c.put(3, 3));
    ZTH_EXPECT_FALSE(c.contains(1));  // 1 was still the oldest
    ZTH_EXPECT_TRUE(c.contains(2));
}
ZTH_TEST("works", "erase removes an entry from both structures and frees its place") {
    Cache c(3);
    c.put(1, 1);
    c.put(2, 2);
    c.put(3, 3);
    ZTH_EXPECT_TRUE(c.erase(2));
    ZTH_EXPECT_FALSE(c.erase(2));
    ZTH_EXPECT_FALSE(c.contains(2));
    ZTH_EXPECT_EQ(c.size(), std::size_t{2});
    ZTH_EXPECT_EQ(show(order(c)), std::string("3 1 "));
    ZTH_EXPECT_FALSE(c.put(4, 4));  // room again: nothing evicted
    ZTH_EXPECT_EQ(c.size(), std::size_t{3});
    ZTH_EXPECT_TRUE(c.erase(4));    // the freshest
    ZTH_EXPECT_TRUE(c.erase(1));    // the oldest
    ZTH_EXPECT_EQ(show(order(c)), std::string("3 "));
    ZTH_EXPECT_TRUE(c.put(1, 1) == false);
    ZTH_EXPECT_EQ(show(order(c)), std::string("1 3 "));
}
ZTH_TEST("works", "clear empties the cache, keeps the statistics, and the cache works afterwards") {
    Cache c(2);
    c.put(1, 1);
    c.get(1);
    c.get(5);
    c.clear();
    ZTH_EXPECT_TRUE(c.empty());
    ZTH_EXPECT_FALSE(c.contains(1));
    ZTH_EXPECT_TRUE(c.get(1) == nullptr);   // a miss
    ZTH_EXPECT_EQ(c.hits(), std::size_t{1});
    ZTH_EXPECT_EQ(c.misses(), std::size_t{2});
    ZTH_EXPECT_FALSE(c.put(1, 7));
    ZTH_EXPECT_EQ(*c.get(1), 7);
}
ZTH_TEST("works", "a cache of capacity 0 stores nothing, and a cache of capacity 1 keeps the last one") {
    Cache z(0);
    ZTH_EXPECT_FALSE(z.put(1, 1));
    ZTH_EXPECT_EQ(z.size(), std::size_t{0});
    ZTH_EXPECT_FALSE(z.contains(1));
    ZTH_EXPECT_TRUE(z.get(1) == nullptr);
    Cache o(1);
    ZTH_EXPECT_FALSE(o.put(1, 1));
    ZTH_EXPECT_TRUE(o.put(2, 2));
    ZTH_EXPECT_TRUE(o.put(3, 3));
    ZTH_EXPECT_FALSE(o.put(3, 33));
    ZTH_EXPECT_EQ(o.size(), std::size_t{1});
    ZTH_EXPECT_EQ(show(order(o)), std::string("3 "));
    ZTH_EXPECT_EQ(*o.get(3), 33);
}
ZTH_TEST("works", "hits and misses are counted by get only") {
    Cache c(2);
    c.put(1, 1);
    c.get(1);
    c.get(1);
    c.get(2);
    c.get(3);
    c.get(4);
    ZTH_EXPECT_EQ(c.hits(), std::size_t{2});
    ZTH_EXPECT_EQ(c.misses(), std::size_t{3});
}
ZTH_TEST("works", "string keys, and values that can only be moved") {
    LruCache<std::string, std::unique_ptr<int>> c(2);
    c.put("a", std::make_unique<int>(1));
    c.put("b", std::make_unique<int>(2));
    c.put("c", std::make_unique<int>(3));
    ZTH_EXPECT_FALSE(c.contains("a"));
    auto* p = c.get("b");
    ZTH_EXPECT_TRUE(p != nullptr);
    if (p != nullptr) ZTH_EXPECT_EQ(**p, 2);
    ZTH_EXPECT_TRUE(c.put("a", std::make_unique<int>(4)));
    ZTH_EXPECT_FALSE(c.contains("c"));
}

// ================================================ Robust ======================================================
ZTH_TEST("robust", "a pointer from get stays valid while other entries come and go (it is the same list node)") {
    Cache c(5);
    for (int i = 1; i <= 5; ++i) c.put(i, i * 10);
    int* p = c.get(3);
    for (int i = 0; i < 4; ++i) {
        c.get(1 + i % 2);        // reorder the others
        c.put(100 + i, 0);       // evicts from the old end, not entry 3, because it was promoted ...
        c.get(3);                // ... and kept fresh
    }
    ZTH_EXPECT_TRUE(c.get(3) == p);
    ZTH_EXPECT_EQ(*p, 30);
    *p = 31;
    ZTH_EXPECT_EQ(*c.peek(3), 31);
}
ZTH_TEST("robust", "30000 random operations agree with the obvious reference, for several capacities") {
    for (std::size_t cap : {std::size_t{0}, std::size_t{1}, std::size_t{2}, std::size_t{5}, std::size_t{17}}) {
        Cache c(cap);
        Reference ref(cap);
        zth::Lcg rng(1234 + cap);
        const int keys = static_cast<int>(cap) * 2 + 3;
        for (int step = 0; step < 6000; ++step) {
            const int k = rng.below(keys);
            const int op = rng.below(10);
            if (op < 4) {
                const int v = rng.below(1000);
                const bool a = c.put(k, v);
                const bool b = ref.put(k, v);
                if (a != b) {
                    zth::fail("put(" + std::to_string(k) + ") returned the wrong 'evicted' flag at step " + std::to_string(step) + " (capacity " + std::to_string(cap) + ")");
                    return;
                }
            } else if (op < 7) {
                const int* got = c.get(k);
                const int i = ref.find(k);
                if ((got != nullptr) != (i >= 0) || (got != nullptr && *got != ref.e[static_cast<std::size_t>(i)].second)) {
                    zth::fail("get(" + std::to_string(k) + ") disagrees with the reference at step " + std::to_string(step));
                    return;
                }
                if (i >= 0) ref.promote(i);
            } else if (op == 7) {
                const bool a = c.erase(k);
                const int i = ref.find(k);
                if (a != (i >= 0)) {
                    zth::fail("erase(" + std::to_string(k) + ") disagrees with the reference at step " + std::to_string(step));
                    return;
                }
                if (i >= 0) ref.e.erase(ref.e.begin() + i);
            } else if (op == 8) {
                if (c.contains(k) != (ref.find(k) >= 0) || (c.peek(k) != nullptr) != (ref.find(k) >= 0)) {
                    zth::fail("contains/peek(" + std::to_string(k) + ") disagrees with the reference at step " + std::to_string(step));
                    return;
                }
            } else if (step % 50 == 0 && rng.below(40) == 0) {
                c.clear();
                ref.e.clear();
            }
            if (c.size() != ref.e.size()) {
                zth::fail("size is " + std::to_string(c.size()) + ", expected " + std::to_string(ref.e.size()) + " at step " + std::to_string(step));
                return;
            }
            if (step % 7 == 0) {
                std::vector<int> expected;
                for (const auto& p : ref.e) expected.push_back(p.first);
                if (order(c) != expected) {
                    zth::fail("recency order is `" + show(order(c)) + "`, expected `" + show(expected) + "` at step " + std::to_string(step));
                    return;
                }
            }
        }
    }
}
ZTH_TEST("robust", "a long stream keeps exactly the most recent keys") {
    Cache c(100);
    for (int i = 0; i < 10000; ++i) c.put(i, i);
    ZTH_EXPECT_EQ(c.size(), std::size_t{100});
    for (int i = 0; i < 9900; ++i) {
        if (c.contains(i)) {
            zth::fail("key " + std::to_string(i) + " should have been evicted");
            return;
        }
    }
    for (int i = 9900; i < 10000; ++i) {
        if (!c.contains(i) || *c.peek(i) != i) {
            zth::fail("key " + std::to_string(i) + " should be present");
            return;
        }
    }
}
ZTH_TEST("robust", "the same key put again and again never grows the cache") {
    Cache c(3);
    for (int i = 0; i < 1000; ++i) ZTH_EXPECT_FALSE(c.put(7, i));
    ZTH_EXPECT_EQ(c.size(), std::size_t{1});
    ZTH_EXPECT_EQ(*c.get(7), 999);
}
ZTH_TEST("robust", "the cache can be copied around by its contents: a value with its own heap data survives eviction and reinsertion") {
    LruCache<int, std::string> c(2);
    c.put(1, std::string(200, 'a'));
    c.put(2, std::string(200, 'b'));
    c.put(3, std::string(200, 'c'));
    c.put(1, std::string(200, 'd'));
    ZTH_EXPECT_EQ(*c.get(1), std::string(200, 'd'));
    ZTH_EXPECT_EQ(*c.get(3), std::string(200, 'c'));
    ZTH_EXPECT_FALSE(c.contains(2));
}

// ================================================ Fast ========================================================
ZTH_TEST("fast", "get is ONE hash lookup: a single hash and about one comparison, in a cache of 5000 entries") {
    LruCache<Key, int, KeyHash> c(5000);
    for (int i = 0; i < 5000; ++i) c.put(Key(i), i);
    Key::reset();
    long found = 0;
    for (int i = 0; i < 5000; ++i) found += c.get(Key(i)) != nullptr;
    ZTH_EXPECT_EQ(found, 5000L);
    ZTH_EXPECT_TRUE(Key::hashes <= 5000);
    ZTH_EXPECT_TRUE(Key::eqs <= 5000 * 3L);
    Key::reset();
    for (int i = 5000; i < 7000; ++i) c.get(Key(i));  // misses
    ZTH_EXPECT_TRUE(Key::hashes <= 2000);
    ZTH_EXPECT_TRUE(Key::eqs <= 2000 * 3L);
}
ZTH_TEST("fast", "put into a full cache: at most 3 hashes (find, evict, insert), at most one copy of the key and no copy of the value") {
    LruCache<Key, Val, KeyHash> c(1000);
    for (int i = 0; i < 1000; ++i) c.put(Key(i), Val(i));
    Key::reset();
    Val::copies = 0;
    for (int i = 1000; i < 3000; ++i) c.put(Key(i), Val(i));
    ZTH_EXPECT_EQ(c.size(), std::size_t{1000});
    ZTH_EXPECT_TRUE(Key::hashes <= 3 * 2000L);
    ZTH_EXPECT_TRUE(Key::copies <= 2000L);
    ZTH_EXPECT_EQ(Val::copies, 0L);
    ZTH_EXPECT_TRUE(Key::eqs <= 2000 * 8L);
}
ZTH_TEST("fast", "updating an existing key hashes once, copies nothing and allocates nothing") {
    LruCache<Key, Val, KeyHash> c(100);
    for (int i = 0; i < 100; ++i) c.put(Key(i), Val(i));
    Key::reset();
    Val::copies = 0;
    zth::mem::Scope scope;
    for (int round = 0; round < 50; ++round)
        for (int i = 0; i < 100; ++i) c.put(Key(i), Val(round));
    ZTH_EXPECT_ALLOCS_AT_MOST(scope, 0);
    ZTH_EXPECT_TRUE(Key::hashes <= 5000L);
    ZTH_EXPECT_EQ(Key::copies, 0L);
    ZTH_EXPECT_EQ(Val::copies, 0L);
}
ZTH_TEST("fast", "get and peek allocate nothing") {
    Cache c(1000);
    for (int i = 0; i < 1000; ++i) c.put(i, i);
    zth::mem::Scope scope;
    long sum = 0;
    for (int round = 0; round < 20; ++round)
        for (int i = 0; i < 1000; ++i) {
            const int* p = c.get(i);
            sum += p ? *p : 0;
            sum += c.peek(i) != nullptr;
        }
    ZTH_EXPECT_ALLOCS_AT_MOST(scope, 0);
    ZTH_EXPECT_TRUE(sum > 0);
}
ZTH_TEST("fast", "a put of a new key allocates at most two nodes (the list node and the map node)") {
    Cache c(1000);
    for (int i = 0; i < 1000; ++i) c.put(i, i);
    zth::mem::Scope scope;
    for (int i = 1000; i < 3000; ++i) c.put(i, i);
    ZTH_EXPECT_ALLOCS_AT_MOST(scope, 2 * 2000);
}
ZTH_TEST_TIMEOUT("fast", "400000 mixed operations on a cache of 30000 entries finish at once (O(1), not O(n))", 20) {
    Cache c(30000);
    zth::Lcg rng(77);
    long hits = 0;
    for (int i = 0; i < 400000; ++i) {
        const int k = rng.below(45000);
        if (rng.below(3) == 0) c.put(k, i);
        else hits += c.get(k) != nullptr;
    }
    ZTH_EXPECT_TRUE(hits > 0);
    ZTH_EXPECT_TRUE(c.size() <= 30000);
}

// ================================================ Clean =======================================================
ZTH_TEST("clean", "no raw memory, no exceptions, no goto") {
    ZTH_EXPECT_NO_RAW_MEMORY("lru.hpp");
    const std::string code = ::zth::strip_comments_and_literals(::zth::read_file("lru.hpp"));
    for (const char* word : {"throw", "try", "catch", "goto"}) ZTH_EXPECT_TRUE(::zth::find_word(code, word).empty());
}
ZTH_TEST("clean", "no linear searching: no find_if, count_if, any_of, std::find over the list") {
    const std::string code = ::zth::strip_comments_and_literals(::zth::read_file("lru.hpp"));
    for (const char* word : {"find_if", "count_if", "any_of", "all_of", "none_of", "count", "remove_if"}) ZTH_EXPECT_TRUE(::zth::find_word(code, word).empty());
}
ZTH_TEST("clean", "recency is kept by splice, not by erasing and re-inserting the node") {
    const std::string code = ::zth::strip_comments_and_literals(::zth::read_file("lru.hpp"));
    ZTH_EXPECT_FALSE(::zth::find_word(code, "splice").empty());
}
