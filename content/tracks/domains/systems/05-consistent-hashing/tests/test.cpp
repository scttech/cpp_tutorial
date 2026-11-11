#define ZTH_TRACK_ALLOCATIONS
#include <algorithm>
#include <map>
#include <set>
#include <string>
#include <vector>

#include "hash_ring.hpp"
#include "types.hpp"
#include "zth_test.hpp"

using namespace chash;

namespace {

// An independent reference: a std::map from point hash to node name, rebuilt from scratch.
struct Reference {
    std::size_t vnodes;
    std::map<std::string, std::size_t> weights;
    explicit Reference(std::size_t v) : vnodes(v == 0 ? 1 : v) {}
    std::map<std::uint64_t, std::string> points() const {
        std::map<std::uint64_t, std::string> p;
        for (const auto& kv : weights)
            for (std::size_t i = 0; i < vnodes * kv.second; ++i) p[hash64(vnode_label(kv.first, i))] = kv.first;
        return p;
    }
    std::string owner(const std::map<std::uint64_t, std::string>& p, const std::string& key) const {
        if (p.empty()) return "";
        auto it = p.lower_bound(hash64(key));
        if (it == p.end()) it = p.begin();
        return it->second;
    }
    std::vector<std::string> replicas(const std::map<std::uint64_t, std::string>& p, const std::string& key, std::size_t n) const {
        std::vector<std::string> out;
        if (p.empty()) return out;
        auto it = p.lower_bound(hash64(key));
        for (std::size_t step = 0; step < p.size() && out.size() < n; ++step) {
            if (it == p.end()) it = p.begin();
            if (std::find(out.begin(), out.end(), it->second) == out.end()) out.push_back(it->second);
            ++it;
        }
        return out;
    }
};

std::string key_of(int i) { return "user:session:" + std::to_string(i * 7919 + 13); }

std::string own(const HashRing& r, const std::string& key) {
    const std::string* p = r.lookup(key);
    return p ? *p : "";
}

}  // namespace

// ================================================ Works =======================================================
ZTH_TEST("works", "an empty ring owns nothing") {
    HashRing r;
    ZTH_EXPECT_TRUE(r.lookup("anything") == nullptr);
    ZTH_EXPECT_EQ(r.node_count(), std::size_t{0});
    ZTH_EXPECT_EQ(r.point_count(), std::size_t{0});
    ZTH_EXPECT_TRUE(r.lookup_n("k", 3).empty());
    ZTH_EXPECT_TRUE(r.nodes().empty());
    ZTH_EXPECT_FALSE(r.remove_node("x"));
    ZTH_EXPECT_FALSE(r.contains("x"));
    ZTH_EXPECT_EQ(r.vnodes_per_weight(), std::size_t{100});
}
ZTH_TEST("works", "adding nodes: counts, duplicates and weights") {
    HashRing r;
    ZTH_EXPECT_TRUE(r.add_node("alpha"));
    ZTH_EXPECT_EQ(r.node_count(), std::size_t{1});
    ZTH_EXPECT_EQ(r.point_count(), std::size_t{100});
    ZTH_EXPECT_FALSE(r.add_node("alpha"));
    ZTH_EXPECT_FALSE(r.add_node("alpha", 5));
    ZTH_EXPECT_EQ(r.point_count(), std::size_t{100});
    ZTH_EXPECT_FALSE(r.add_node("zero", 0));
    ZTH_EXPECT_FALSE(r.contains("zero"));
    ZTH_EXPECT_TRUE(r.add_node("beta", 3));
    ZTH_EXPECT_EQ(r.point_count(), std::size_t{400});
    ZTH_EXPECT_EQ(r.node_count(), std::size_t{2});
    ZTH_EXPECT_TRUE(r.contains("alpha"));
    ZTH_EXPECT_TRUE(r.contains("beta"));
    ZTH_EXPECT_FALSE(r.contains("gamma"));
}
ZTH_TEST("works", "the number of virtual nodes is configurable; 0 means 1") {
    HashRing a(10), z(0);
    a.add_node("x", 2);
    z.add_node("x");
    ZTH_EXPECT_EQ(a.point_count(), std::size_t{20});
    ZTH_EXPECT_EQ(a.vnodes_per_weight(), std::size_t{10});
    ZTH_EXPECT_EQ(z.point_count(), std::size_t{1});
    ZTH_EXPECT_EQ(z.vnodes_per_weight(), std::size_t{1});
    ZTH_EXPECT_TRUE(z.lookup("whatever") != nullptr);
}
ZTH_TEST("works", "a single node owns every key") {
    HashRing r;
    r.add_node("only");
    for (int i = 0; i < 500; ++i) {
        const std::string* p = r.lookup(key_of(i));
        ZTH_EXPECT_TRUE(p != nullptr);
        if (p == nullptr) return;
        ZTH_EXPECT_EQ(*p, std::string("only"));
    }
}
ZTH_TEST("works", "lookup agrees with an independent reference: first point at or after the key, clockwise") {
    HashRing r(50);
    Reference ref(50);
    for (const char* n : {"red", "green", "blue", "cyan", "magenta"}) {
        r.add_node(n);
        ref.weights[n] = 1;
    }
    const auto pts = ref.points();
    for (int i = 0; i < 5000; ++i) {
        const std::string key = key_of(i);
        if (own(r, key) != ref.owner(pts, key)) {
            zth::fail("key `" + key + "` is owned by " + own(r, key) + ", the reference says " + ref.owner(pts, key));
            return;
        }
    }
}
ZTH_TEST("works", "keys beyond the last point wrap around to the first point") {
    HashRing r(1);
    Reference ref(1);
    for (const char* n : {"n1", "n2", "n3"}) {
        r.add_node(n);
        ref.weights[n] = 1;
    }
    const auto pts = ref.points();
    const std::uint64_t last = pts.rbegin()->first;
    int checked = 0;
    for (int i = 0; i < 4000 && checked < 3; ++i) {
        const std::string key = "wrap-" + std::to_string(i);
        if (hash64(key) > last) {
            ++checked;
            ZTH_EXPECT_EQ(own(r, key), pts.begin()->second);
        }
    }
    ZTH_EXPECT_TRUE(checked >= 1);   // with 3 points about 1 key in 4 is past the last point
}
ZTH_TEST("works", "nodes() lists the names in alphabetical order") {
    HashRing r;
    for (const char* n : {"delta", "alpha", "charlie", "bravo"}) r.add_node(n);
    const auto names = r.nodes();
    ZTH_EXPECT_EQ(names.size(), std::size_t{4});
    if (names.size() == 4) {
        ZTH_EXPECT_EQ(names[0], std::string("alpha"));
        ZTH_EXPECT_EQ(names[1], std::string("bravo"));
        ZTH_EXPECT_EQ(names[2], std::string("charlie"));
        ZTH_EXPECT_EQ(names[3], std::string("delta"));
    }
}
ZTH_TEST("works", "removing a node removes all its points and none of the others'") {
    HashRing r;
    r.add_node("a");
    r.add_node("b", 2);
    r.add_node("c");
    ZTH_EXPECT_FALSE(r.remove_node("nope"));
    ZTH_EXPECT_TRUE(r.remove_node("b"));
    ZTH_EXPECT_FALSE(r.contains("b"));
    ZTH_EXPECT_EQ(r.node_count(), std::size_t{2});
    ZTH_EXPECT_EQ(r.point_count(), std::size_t{200});
    ZTH_EXPECT_FALSE(r.remove_node("b"));
    for (int i = 0; i < 500; ++i) {
        const std::string o = own(r, key_of(i));
        ZTH_EXPECT_TRUE(o == "a" || o == "c");
    }
    ZTH_EXPECT_TRUE(r.remove_node("a"));
    ZTH_EXPECT_TRUE(r.remove_node("c"));
    ZTH_EXPECT_TRUE(r.lookup("k") == nullptr);
    ZTH_EXPECT_EQ(r.point_count(), std::size_t{0});
    ZTH_EXPECT_TRUE(r.add_node("a"));   // a removed node can come back
    ZTH_EXPECT_EQ(r.point_count(), std::size_t{100});
}
ZTH_TEST("works", "lookup_n returns distinct nodes, the owner first, clockwise") {
    HashRing r(30);
    Reference ref(30);
    for (const char* n : {"w", "x", "y", "z", "q"}) {
        r.add_node(n);
        ref.weights[n] = 1;
    }
    const auto pts = ref.points();
    for (int i = 0; i < 300; ++i) {
        const std::string key = key_of(i);
        for (std::size_t n : {std::size_t{1}, std::size_t{2}, std::size_t{3}, std::size_t{5}}) {
            const auto got = r.lookup_n(key, n);
            const auto want = ref.replicas(pts, key, n);
            if (got != want) {
                zth::fail("the " + std::to_string(n) + " replicas of `" + key + "` are wrong");
                return;
            }
        }
        const auto first = r.lookup_n(key, 1);
        ZTH_EXPECT_EQ(first.size(), std::size_t{1});
        if (first.size() == 1) ZTH_EXPECT_EQ(first.front(), own(r, key));
    }
}
ZTH_TEST("works", "lookup_n edge cases: n == 0, more replicas than nodes, a single node") {
    HashRing r;
    r.add_node("a");
    r.add_node("b");
    ZTH_EXPECT_TRUE(r.lookup_n("k", 0).empty());
    const auto all = r.lookup_n("k", 10);
    ZTH_EXPECT_EQ(all.size(), std::size_t{2});
    ZTH_EXPECT_TRUE(all.size() == 2 && all[0] != all[1]);
    HashRing one;
    one.add_node("solo");
    const auto s = one.lookup_n("k", 3);
    ZTH_EXPECT_EQ(s.size(), std::size_t{1});
}
ZTH_TEST("works", "the ring does not depend on the order in which nodes were added or removed") {
    HashRing a, b, c;
    for (const char* n : {"n1", "n2", "n3", "n4"}) a.add_node(n);
    for (const char* n : {"n4", "n2", "n1", "n3"}) b.add_node(n);
    c.add_node("n3");
    c.add_node("tmp");
    c.add_node("n1", 1);
    c.remove_node("tmp");
    c.add_node("n4");
    c.add_node("n2");
    for (int i = 0; i < 3000; ++i) {
        const std::string key = key_of(i);
        const std::string o = own(a, key);
        if (o != own(b, key) || o != own(c, key) || a.lookup_n(key, 3) != b.lookup_n(key, 3) || a.lookup_n(key, 3) != c.lookup_n(key, 3)) {
            zth::fail("the rings built in different orders disagree about `" + key + "`");
            return;
        }
    }
}
ZTH_TEST("works", "the returned pointer is the node's name and stays valid while the node exists") {
    HashRing r;
    r.add_node("keeper");
    const std::string* p = r.lookup("k");
    ZTH_EXPECT_TRUE(p != nullptr);
    for (int i = 0; i < 200; ++i) r.add_node("filler-" + std::to_string(i));   // forces rehashes and reallocations
    r.remove_node("filler-3");
    const std::string* q = r.lookup("k");
    if (own(r, "k") == "keeper") ZTH_EXPECT_TRUE(p == q);
    ZTH_EXPECT_TRUE(q != nullptr && !q->empty());
}

// ================================================ Robust ======================================================
ZTH_TEST("robust", "adding a node moves keys ONLY to that node, and only about 1/(N+1) of them") {
    HashRing r;
    for (int i = 0; i < 20; ++i) r.add_node("server-" + std::to_string(i));
    const int kKeys = 50000;
    std::vector<std::string> before(kKeys);
    for (int i = 0; i < kKeys; ++i) before[static_cast<std::size_t>(i)] = own(r, key_of(i));
    r.add_node("server-new");
    int moved = 0;
    for (int i = 0; i < kKeys; ++i) {
        const std::string now = own(r, key_of(i));
        if (now != before[static_cast<std::size_t>(i)]) {
            ++moved;
            if (now != "server-new") {
                zth::fail("key " + std::to_string(i) + " moved between old servers: " + before[static_cast<std::size_t>(i)] + " -> " + now);
                return;
            }
        }
    }
    ZTH_EXPECT_TRUE(moved > kKeys * 25 / 1000);   // expected 4.8%
    ZTH_EXPECT_TRUE(moved < kKeys * 80 / 1000);
}
ZTH_TEST("robust", "removing a node moves only the keys it owned, and every other key stays put") {
    HashRing r;
    for (int i = 0; i < 12; ++i) r.add_node("srv" + std::to_string(i));
    const int kKeys = 40000;
    std::vector<std::string> before(kKeys);
    int owned = 0;
    for (int i = 0; i < kKeys; ++i) {
        before[static_cast<std::size_t>(i)] = own(r, key_of(i));
        owned += before[static_cast<std::size_t>(i)] == "srv5";
    }
    r.remove_node("srv5");
    for (int i = 0; i < kKeys; ++i) {
        const std::string now = own(r, key_of(i));
        const std::string& was = before[static_cast<std::size_t>(i)];
        if (was != "srv5" && now != was) {
            zth::fail("key " + std::to_string(i) + " moved although its owner " + was + " is still there");
            return;
        }
        if (was == "srv5" && now == "srv5") {
            zth::fail("a key is still owned by the removed server");
            return;
        }
    }
    ZTH_EXPECT_TRUE(owned > 0);
}
ZTH_TEST("robust", "virtual nodes balance the load: 10 equal servers each own roughly a tenth of 100000 keys") {
    HashRing r;
    for (int i = 0; i < 10; ++i) r.add_node("node-" + std::to_string(i));
    std::map<std::string, int> load;
    for (int i = 0; i < 100000; ++i) ++load[own(r, key_of(i))];
    ZTH_EXPECT_EQ(load.size(), std::size_t{10});
    for (const auto& kv : load) {
        if (kv.second < 5000 || kv.second > 16000) {
            zth::fail(kv.first + " owns " + std::to_string(kv.second) + " of 100000 keys (a fair share is 10000)");
            return;
        }
    }
}
ZTH_TEST("robust", "with ONE virtual node per server the load is badly uneven (this is why virtual nodes exist)") {
    HashRing r(1);
    for (int i = 0; i < 10; ++i) r.add_node("node-" + std::to_string(i));
    std::map<std::string, int> load;
    for (int i = 0; i < 100000; ++i) ++load[own(r, key_of(i))];
    int lo = 100000, hi = 0;
    for (const auto& kv : load) {
        lo = std::min(lo, kv.second);
        hi = std::max(hi, kv.second);
    }
    ZTH_EXPECT_TRUE(hi > 2 * lo);
}
ZTH_TEST("robust", "weights give proportional shares: 1 : 2 : 3") {
    HashRing r;
    r.add_node("small", 1);
    r.add_node("medium", 2);
    r.add_node("large", 3);
    std::map<std::string, int> load;
    for (int i = 0; i < 120000; ++i) ++load[own(r, key_of(i))];
    ZTH_EXPECT_TRUE(load["small"] > 12000 && load["small"] < 30000);    // 1/6 = 20000
    ZTH_EXPECT_TRUE(load["medium"] > 28000 && load["medium"] < 52000);  // 2/6 = 40000
    ZTH_EXPECT_TRUE(load["large"] > 48000 && load["large"] < 72000);    // 3/6 = 60000
    ZTH_EXPECT_TRUE(load["small"] < load["medium"] && load["medium"] < load["large"]);
}
ZTH_TEST("robust", "replica sets are stable: removing a node that is not in a key's replica set does not change it") {
    HashRing r;
    for (int i = 0; i < 9; ++i) r.add_node("db" + std::to_string(i));
    std::vector<std::vector<std::string>> before;
    for (int i = 0; i < 2000; ++i) before.push_back(r.lookup_n(key_of(i), 3));
    r.remove_node("db4");
    for (int i = 0; i < 2000; ++i) {
        const auto& old = before[static_cast<std::size_t>(i)];
        if (std::find(old.begin(), old.end(), "db4") != old.end()) continue;
        if (r.lookup_n(key_of(i), 3) != old) {
            zth::fail("the replica set of key " + std::to_string(i) + " changed although db4 was not in it");
            return;
        }
    }
}
ZTH_TEST("robust", "random adds, removes and lookups agree with a reference rebuilt from scratch") {
    zth::Lcg rng(777);
    HashRing r(20);
    Reference ref(20);
    for (int step = 0; step < 400; ++step) {
        const std::string name = "node" + std::to_string(rng.below(25));
        const int op = rng.below(10);
        if (op < 4) {
            const std::size_t w = static_cast<std::size_t>(1 + rng.below(4));
            const bool added = r.add_node(name, w);
            ZTH_EXPECT_EQ(added, ref.weights.count(name) == 0);
            if (added) ref.weights[name] = w;
        } else if (op < 6) {
            const bool removed = r.remove_node(name);
            ZTH_EXPECT_EQ(removed, ref.weights.count(name) == 1);
            ref.weights.erase(name);
        }
        const auto pts = ref.points();
        ZTH_EXPECT_EQ(r.point_count(), pts.size());
        ZTH_EXPECT_EQ(r.node_count(), ref.weights.size());
        for (int k = 0; k < 20; ++k) {
            const std::string key = "k" + std::to_string(rng.below(100000));
            if (own(r, key) != ref.owner(pts, key) || r.lookup_n(key, 3) != ref.replicas(pts, key, 3)) {
                zth::fail("step " + std::to_string(step) + ": the ring disagrees with the reference about `" + key + "`");
                return;
            }
        }
    }
}

// ================================================ Fast ========================================================
ZTH_TEST("fast", "lookups allocate nothing, even for keys longer than the small-string buffer") {
    HashRing r;
    for (int i = 0; i < 20; ++i) r.add_node("server-" + std::to_string(i));
    const std::string long_key = "a-key-that-is-longer-than-any-small-string-buffer-could-hold-";
    std::vector<std::string> keys;
    for (int i = 0; i < 100; ++i) keys.push_back(long_key + std::to_string(i));
    zth::mem::Scope scope;
    std::size_t total = 0;
    for (int round = 0; round < 1000; ++round)
        for (const std::string& k : keys) total += r.lookup(k)->size();
    ZTH_EXPECT_ALLOCS_AT_MOST(scope, 0);
    ZTH_EXPECT_TRUE(total > 0);
}
ZTH_TEST_TIMEOUT("fast", "a lookup is a binary search: 2 million lookups on a ring of 100000 points", 20) {
    HashRing r;
    for (int i = 0; i < 1000; ++i) r.add_node("server-" + std::to_string(i));
    ZTH_EXPECT_EQ(r.point_count(), std::size_t{100000});
    std::size_t hits = 0;
    for (int i = 0; i < 2000000; ++i) hits += r.lookup(key_of(i)) != nullptr;
    ZTH_EXPECT_EQ(hits, std::size_t{2000000});
}
ZTH_TEST_TIMEOUT("fast", "adding a node merges instead of re-sorting: 1500 servers with 100 virtual nodes each", 20) {
    HashRing r;
    for (int i = 0; i < 1500; ++i) ZTH_EXPECT_TRUE(r.add_node("server-" + std::to_string(i)));
    ZTH_EXPECT_EQ(r.point_count(), std::size_t{150000});
    ZTH_EXPECT_TRUE(r.lookup("k") != nullptr);
}
ZTH_TEST_TIMEOUT("fast", "removing nodes is one pass over the points", 20) {
    HashRing r;
    for (int i = 0; i < 600; ++i) r.add_node("server-" + std::to_string(i));
    for (int i = 0; i < 600; i += 2) ZTH_EXPECT_TRUE(r.remove_node("server-" + std::to_string(i)));
    ZTH_EXPECT_EQ(r.node_count(), std::size_t{300});
    ZTH_EXPECT_EQ(r.point_count(), std::size_t{30000});
    for (int i = 0; i < 20000; ++i) {
        const std::string* p = r.lookup(key_of(i));
        if (p == nullptr || std::stoi(p->substr(7)) % 2 == 0) {
            zth::fail("a removed (even-numbered) server still owns a key");
            return;
        }
    }
}
ZTH_TEST("fast", "lookup_n allocates only its result") {
    HashRing r;
    for (int i = 0; i < 10; ++i) r.add_node("srv" + std::to_string(i));
    zth::mem::Scope scope;
    for (int i = 0; i < 500; ++i) {
        const auto v = r.lookup_n("some-key-with-a-long-enough-name-" + std::to_string(i), 3);
        (void)v;
    }
    // per call: the key string (1) + the result vector (1, reserved once) + names that fit the small-string buffer (0)
    ZTH_EXPECT_ALLOCS_AT_MOST(scope, 500 * 3);
}

// ================================================ Clean =======================================================
ZTH_TEST("clean", "no raw memory, no exceptions, no goto") {
    ZTH_EXPECT_NO_RAW_MEMORY("hash_ring.hpp");
    const std::string code = ::zth::strip_comments_and_literals(::zth::read_file("hash_ring.hpp"));
    for (const char* word : {"throw", "try", "catch", "goto"}) ZTH_EXPECT_TRUE(::zth::find_word(code, word).empty());
}
ZTH_TEST("clean", "the ring is a sorted contiguous array searched with lower_bound: no std::map, set or list for the points") {
    const std::string code = ::zth::strip_comments_and_literals(::zth::read_file("hash_ring.hpp"));
    for (const char* word : {"map", "multimap", "set", "multiset", "list", "forward_list", "deque", "find_if"}) ZTH_EXPECT_TRUE(::zth::find_word(code, word).empty());
    ZTH_EXPECT_FALSE(::zth::find_word(code, "lower_bound").empty());
}
ZTH_TEST("clean", "hashing goes through hash64 and vnode_label from types.hpp") {
    const std::string code = ::zth::strip_comments_and_literals(::zth::read_file("hash_ring.hpp"));
    ZTH_EXPECT_FALSE(::zth::find_word(code, "hash64").empty());
    ZTH_EXPECT_FALSE(::zth::find_word(code, "vnode_label").empty());
}
