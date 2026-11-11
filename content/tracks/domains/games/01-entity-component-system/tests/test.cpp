#define ZTH_TRACK_ALLOCATIONS
#include <algorithm>
#include <map>
#include <set>
#include <string>
#include <vector>

#include "registry.hpp"
#include "zth_test.hpp"

using namespace ecs;

// ================================================ Works =======================================================
ZTH_TEST("works", "create gives distinct live entities; the null entity is not alive") {
    Registry r;
    const Entity a = r.create(), b = r.create(), c = r.create();
    ZTH_EXPECT_TRUE(a.index != b.index && b.index != c.index);
    ZTH_EXPECT_TRUE(r.alive(a) && r.alive(b) && r.alive(c));
    ZTH_EXPECT_FALSE(r.alive(kNullEntity));
    ZTH_EXPECT_TRUE(a == a);
    ZTH_EXPECT_FALSE(a == b);
}
ZTH_TEST("works", "add / get / has / remove for components of different types") {
    Registry r;
    const Entity e = r.create();
    ZTH_EXPECT_FALSE(r.has<Position>(e));
    ZTH_EXPECT_TRUE(r.get<Position>(e) == nullptr);
    Position* p = r.add(e, Position{1.5f, 2.5f});
    ZTH_EXPECT_TRUE(p != nullptr);
    ZTH_EXPECT_TRUE(r.has<Position>(e));
    ZTH_EXPECT_FALSE(r.has<Velocity>(e));
    ZTH_EXPECT_EQ(r.get<Position>(e)->x, 1.5f);
    ZTH_EXPECT_EQ(r.get<Position>(e)->y, 2.5f);
    r.add(e, Health{7});
    ZTH_EXPECT_EQ(r.get<Health>(e)->hp, 7);
    ZTH_EXPECT_TRUE(r.remove<Position>(e));
    ZTH_EXPECT_FALSE(r.has<Position>(e));
    ZTH_EXPECT_FALSE(r.remove<Position>(e));
    ZTH_EXPECT_TRUE(r.has<Health>(e));
    r.get<Health>(e)->hp = 9;
    ZTH_EXPECT_EQ(r.get<Health>(e)->hp, 9);
}
ZTH_TEST("works", "adding a component again replaces its value; count counts entities with the component") {
    Registry r;
    const Entity a = r.create(), b = r.create();
    r.add(a, Health{1});
    r.add(b, Health{2});
    r.add(a, Health{50});
    ZTH_EXPECT_EQ(r.count<Health>(), std::size_t{2});
    ZTH_EXPECT_EQ(r.get<Health>(a)->hp, 50);
    ZTH_EXPECT_EQ(r.get<Health>(b)->hp, 2);
    ZTH_EXPECT_EQ(r.count<Position>(), std::size_t{0});
}
ZTH_TEST("works", "a tag component (no data) works like any other") {
    Registry r;
    const Entity a = r.create(), b = r.create();
    r.add(a, Frozen{});
    ZTH_EXPECT_TRUE(r.has<Frozen>(a));
    ZTH_EXPECT_FALSE(r.has<Frozen>(b));
    ZTH_EXPECT_EQ(r.count<Frozen>(), std::size_t{1});
}
ZTH_TEST("works", "each visits exactly the entities that have all the requested components, with the right data") {
    Registry r;
    std::vector<Entity> es;
    for (int i = 0; i < 10; ++i) {
        es.push_back(r.create());
        r.add(es.back(), Position{static_cast<float>(i), 0});
        if (i % 2 == 0) r.add(es.back(), Velocity{1, static_cast<float>(i)});
        if (i % 3 == 0) r.add(es.back(), Health{i});
    }
    std::map<EntityId, float> seen;
    r.each<Position, Velocity>([&](Entity e, Position& p, Velocity& v) {
        seen[e.index] = p.x + v.dy;
        p.y = 99;
    });
    ZTH_EXPECT_EQ(seen.size(), std::size_t{5});
    for (int i = 0; i < 10; i += 2) ZTH_EXPECT_EQ(seen.at(es[static_cast<std::size_t>(i)].index), static_cast<float>(i) * 2);
    ZTH_EXPECT_EQ(r.get<Position>(es[2])->y, 99.0f);  // changes through the reference stick
    ZTH_EXPECT_EQ(r.get<Position>(es[3])->y, 0.0f);   // entities that were not visited are untouched
    int three = 0;
    r.each<Position, Velocity, Health>([&](Entity, Position&, Velocity&, Health&) { ++three; });
    ZTH_EXPECT_EQ(three, 2);  // 0 and 6
    int one = 0;
    r.each<Health>([&](Entity e, Health& h) {
        ++one;
        ZTH_EXPECT_EQ(h.hp, static_cast<int>(e.index));
    });
    ZTH_EXPECT_EQ(one, 4);  // 0 3 6 9
}
ZTH_TEST("works", "each passes the entity handle: it is alive and the components can be looked up through it") {
    Registry r;
    const Entity a = r.create();
    r.add(a, Position{4, 5});
    r.add(a, Velocity{1, 1});
    int n = 0;
    r.each<Position>([&](Entity e, Position&) {
        ++n;
        ZTH_EXPECT_TRUE(e == a);
        ZTH_EXPECT_TRUE(r.alive(e));
        ZTH_EXPECT_TRUE(r.has<Velocity>(e));
    });
    ZTH_EXPECT_EQ(n, 1);
}
ZTH_TEST("works", "destroy removes all components and frees the slot; the slot is reused with a higher generation") {
    Registry r;
    const Entity a = r.create(), b = r.create();
    r.add(a, Position{1, 1});
    r.add(a, Health{5});
    r.add(b, Position{2, 2});
    r.destroy(a);
    ZTH_EXPECT_FALSE(r.alive(a));
    ZTH_EXPECT_TRUE(r.alive(b));
    ZTH_EXPECT_EQ(r.count<Position>(), std::size_t{1});
    ZTH_EXPECT_EQ(r.count<Health>(), std::size_t{0});
    const Entity c = r.create();
    ZTH_EXPECT_EQ(c.index, a.index);
    ZTH_EXPECT_EQ(c.generation, a.generation + 1);
    ZTH_EXPECT_FALSE(c == a);
    ZTH_EXPECT_FALSE(r.has<Position>(c));  // the new entity starts empty
    ZTH_EXPECT_FALSE(r.has<Health>(c));
}
ZTH_TEST("works", "a stale handle is not alive and cannot reach the new entity's components") {
    Registry r;
    const Entity old = r.create();
    r.add(old, Health{1});
    r.destroy(old);
    const Entity fresh = r.create();
    r.add(fresh, Health{77});
    ZTH_EXPECT_FALSE(r.alive(old));
    ZTH_EXPECT_TRUE(r.get<Health>(old) == nullptr);
    ZTH_EXPECT_FALSE(r.has<Health>(old));
    ZTH_EXPECT_TRUE(r.add(old, Health{5}) == nullptr);
    ZTH_EXPECT_FALSE(r.remove<Health>(old));
    r.destroy(old);  // must not destroy `fresh`
    ZTH_EXPECT_TRUE(r.alive(fresh));
    ZTH_EXPECT_EQ(r.get<Health>(fresh)->hp, 77);
}
ZTH_TEST("works", "SparseSet used directly: dense arrays stay aligned, erase is swap-and-pop") {
    SparseSet<Health> s;
    for (EntityId id : {10u, 20u, 30u, 40u}) s.emplace(id, Health{static_cast<int>(id)});
    ZTH_EXPECT_EQ(s.size(), std::size_t{4});
    ZTH_EXPECT_TRUE(s.contains(30));
    ZTH_EXPECT_FALSE(s.contains(31));
    ZTH_EXPECT_TRUE(s.erase(20));
    ZTH_EXPECT_FALSE(s.erase(20));
    ZTH_EXPECT_EQ(s.size(), std::size_t{3});
    ZTH_EXPECT_EQ(s.entities().size(), s.components().size());
    // the last element (40) moved into the hole
    ZTH_EXPECT_EQ(s.entities()[1], 40u);
    ZTH_EXPECT_EQ(s.components()[1].hp, 40);
    for (EntityId id : {10u, 30u, 40u}) {
        ZTH_EXPECT_TRUE(s.find(id) != nullptr);
        ZTH_EXPECT_EQ(s.find(id)->hp, static_cast<int>(id));
    }
    ZTH_EXPECT_TRUE(s.find(20) == nullptr);
    s.clear();
    ZTH_EXPECT_EQ(s.size(), std::size_t{0});
    ZTH_EXPECT_FALSE(s.contains(10));
}

// ================================================ Robust ======================================================
ZTH_TEST("robust", "SparseSet: replace instead of duplicate; erase first, last and only element; a large id; contains after erase") {
    SparseSet<Health> s;
    s.emplace(5, Health{1});
    s.emplace(5, Health{2});
    ZTH_EXPECT_EQ(s.size(), std::size_t{1});
    ZTH_EXPECT_EQ(s.find(5)->hp, 2);
    ZTH_EXPECT_TRUE(s.erase(5));  // the only element
    ZTH_EXPECT_EQ(s.size(), std::size_t{0});
    ZTH_EXPECT_FALSE(s.contains(5));
    s.emplace(1, Health{10});
    s.emplace(2, Health{20});
    s.emplace(3, Health{30});
    ZTH_EXPECT_TRUE(s.erase(1));  // the first
    ZTH_EXPECT_TRUE(s.erase(3));  // the last
    ZTH_EXPECT_EQ(s.size(), std::size_t{1});
    ZTH_EXPECT_EQ(s.find(2)->hp, 20);
    ZTH_EXPECT_FALSE(s.contains(1));
    ZTH_EXPECT_FALSE(s.contains(3));
    s.emplace(1000000, Health{99});
    ZTH_EXPECT_TRUE(s.contains(1000000));
    ZTH_EXPECT_FALSE(s.contains(999999));
    ZTH_EXPECT_FALSE(s.contains(2000000));
    ZTH_EXPECT_EQ(s.find(1000000)->hp, 99);
    ZTH_EXPECT_FALSE(s.contains(0xFFFFFFF0u));
}
ZTH_TEST("robust", "SparseSet: a stale sparse entry (the slot now holds another entity) does not make contains() lie") {
    SparseSet<Health> s;
    s.emplace(0, Health{1});
    s.emplace(1, Health{2});
    s.emplace(2, Health{3});
    s.erase(0);  // 2 moves to slot 0; the old sparse entry of 0 still says slot 0
    ZTH_EXPECT_FALSE(s.contains(0));
    ZTH_EXPECT_TRUE(s.contains(2));
    ZTH_EXPECT_EQ(s.find(2)->hp, 3);
    ZTH_EXPECT_TRUE(s.find(0) == nullptr);
    s.erase(1);
    s.erase(2);
    ZTH_EXPECT_EQ(s.size(), std::size_t{0});
    ZTH_EXPECT_FALSE(s.contains(1));
    ZTH_EXPECT_FALSE(s.contains(2));
}
ZTH_TEST("robust", "SparseSet: 4000 random operations agree with a std::map") {
    ::zth::Lcg rng(12345);
    SparseSet<Health> s;
    std::map<EntityId, int> model;
    for (int step = 0; step < 4000; ++step) {
        const EntityId id = static_cast<EntityId>(rng.below(60));
        switch (rng.below(3)) {
            case 0: s.emplace(id, Health{step}); model[id] = step; break;
            case 1: ZTH_EXPECT_EQ(s.erase(id), model.erase(id) > 0); break;
            default: ZTH_EXPECT_EQ(s.contains(id), model.count(id) > 0);
        }
        ZTH_EXPECT_EQ(s.size(), model.size());
    }
    for (const auto& kv : model) {
        ZTH_EXPECT_TRUE(s.find(kv.first) != nullptr);
        ZTH_EXPECT_EQ(s.find(kv.first)->hp, kv.second);
    }
    ZTH_EXPECT_EQ(s.entities().size(), s.components().size());
    std::set<EntityId> dense(s.entities().begin(), s.entities().end());
    ZTH_EXPECT_EQ(dense.size(), s.size());  // no duplicates in the dense array
}
ZTH_TEST("robust", "registry: dead, null and never-created handles are handled everywhere") {
    Registry r;
    const Entity a = r.create();
    r.destroy(a);
    r.destroy(a);  // twice
    r.destroy(kNullEntity);
    const Entity fake{12345, 0};
    for (const Entity e : {a, kNullEntity, fake}) {
        ZTH_EXPECT_FALSE(r.alive(e));
        ZTH_EXPECT_TRUE(r.add(e, Position{}) == nullptr);
        ZTH_EXPECT_TRUE(r.get<Position>(e) == nullptr);
        ZTH_EXPECT_FALSE(r.has<Position>(e));
        ZTH_EXPECT_FALSE(r.remove<Position>(e));
    }
    ZTH_EXPECT_EQ(r.count<Position>(), std::size_t{0});
    const Entity b = r.create();
    ZTH_EXPECT_TRUE(r.alive(b));
    ZTH_EXPECT_EQ(b.index, a.index);  // the double destroy freed the slot only once
    const Entity c = r.create();
    ZTH_EXPECT_TRUE(c.index != b.index);
}
ZTH_TEST("robust", "slots are reused: 20000 create/destroy cycles with at most 3 live entities never use a slot above 10") {
    Registry r;
    std::vector<Entity> live;
    ::zth::Lcg rng(9);
    EntityId highest = 0;
    for (int i = 0; i < 20000; ++i) {
        if (live.size() < 3 && rng.below(2) == 0) {
            live.push_back(r.create());
            highest = std::max(highest, live.back().index);
            r.add(live.back(), Health{i});
        } else if (!live.empty()) {
            const std::size_t k = static_cast<std::size_t>(rng.below(static_cast<int>(live.size())));
            r.destroy(live[k]);
            live.erase(live.begin() + static_cast<std::ptrdiff_t>(k));
        }
    }
    ZTH_EXPECT_TRUE(highest <= 10);
    ZTH_EXPECT_EQ(r.count<Health>(), live.size());
}
ZTH_TEST("robust", "each with a component nobody has, on an empty registry, and with entities that match nothing") {
    Registry r;
    int calls = 0;
    r.each<Position>([&](Entity, Position&) { ++calls; });
    r.each<Position, Velocity>([&](Entity, Position&, Velocity&) { ++calls; });
    const Entity a = r.create(), b = r.create();
    r.add(a, Position{});
    r.add(b, Velocity{});
    r.each<Position, Velocity>([&](Entity, Position&, Velocity&) { ++calls; });
    r.each<Health>([&](Entity, Health&) { ++calls; });
    r.each<Position, Frozen>([&](Entity, Position&, Frozen&) { ++calls; });  // Position exists, nobody ever had a Frozen
    r.each<Frozen, Position>([&](Entity, Frozen&, Position&) { ++calls; });
    ZTH_EXPECT_EQ(calls, 0);
    r.each<Position>([&](Entity, Position&) { ++calls; });
    ZTH_EXPECT_EQ(calls, 1);
}
ZTH_TEST("robust", "each gives the handle with the CURRENT generation of reused slots") {
    Registry r;
    Entity a = r.create();
    r.destroy(a);
    a = r.create();  // generation 1
    r.add(a, Position{});
    int n = 0;
    r.each<Position>([&](Entity e, Position&) {
        ++n;
        ZTH_EXPECT_EQ(e.generation, 1u);
        ZTH_EXPECT_TRUE(r.alive(e));
    });
    ZTH_EXPECT_EQ(n, 1);
}
ZTH_TEST("robust", "components of destroyed entities never show up in each, also after the slot was reused") {
    Registry r;
    std::vector<Entity> es;
    for (int i = 0; i < 20; ++i) {
        es.push_back(r.create());
        r.add(es.back(), Position{static_cast<float>(i), 0});
        r.add(es.back(), Velocity{});
    }
    for (int i = 0; i < 20; i += 3) r.destroy(es[static_cast<std::size_t>(i)]);
    for (int i = 0; i < 4; ++i) r.create();  // reuse some slots, without components
    int n = 0;
    std::set<EntityId> seen;
    r.each<Position, Velocity>([&](Entity e, Position&, Velocity&) {
        ++n;
        seen.insert(e.index);
        ZTH_EXPECT_TRUE(r.alive(e));
    });
    ZTH_EXPECT_EQ(n, 13);
    ZTH_EXPECT_EQ(seen.size(), std::size_t{13});
}
ZTH_TEST_TIMEOUT("robust", "a random mix of create / destroy / add / remove / each agrees with a simple model", 10) {
    ::zth::Lcg rng(777);
    Registry r;
    struct Model {
        Entity e;
        bool alive = true;
        int hp = -1;
        bool has_pos = false;
    };
    std::vector<Model> m;
    for (int step = 0; step < 4000; ++step) {
        switch (rng.below(6)) {
            case 0: m.push_back({r.create(), true, -1, false}); break;
            case 1:
                if (!m.empty()) {
                    Model& x = m[static_cast<std::size_t>(rng.below(static_cast<int>(m.size())))];
                    r.destroy(x.e);
                    x.alive = false;
                }
                break;
            case 2:
                if (!m.empty()) {
                    Model& x = m[static_cast<std::size_t>(rng.below(static_cast<int>(m.size())))];
                    Health* h = r.add(x.e, Health{step});
                    ZTH_EXPECT_EQ(h != nullptr, x.alive);
                    if (x.alive) x.hp = step;
                }
                break;
            case 3:
                if (!m.empty()) {
                    Model& x = m[static_cast<std::size_t>(rng.below(static_cast<int>(m.size())))];
                    ZTH_EXPECT_EQ(r.remove<Health>(x.e), x.alive && x.hp >= 0);
                    if (x.alive) x.hp = -1;
                }
                break;
            case 4:
                if (!m.empty()) {
                    Model& x = m[static_cast<std::size_t>(rng.below(static_cast<int>(m.size())))];
                    if (x.alive) {
                        r.add(x.e, Position{});
                        x.has_pos = true;
                    }
                }
                break;
            default: {
                std::set<EntityId> want, got;
                for (const Model& x : m) {
                    if (x.alive && x.hp >= 0 && x.has_pos) want.insert(x.e.index);
                }
                r.each<Health, Position>([&](Entity e, Health&, Position&) { got.insert(e.index); });
                ZTH_EXPECT_TRUE(want == got);
            }
        }
    }
}

// ================================================ Fast ========================================================
ZTH_TEST_TIMEOUT("fast", "adding components moves each one at most 4 times on average (100000 adds; no copies)", 10) {
    Registry r;
    std::vector<Entity> es;
    for (int i = 0; i < 100000; ++i) es.push_back(r.create());
    stats::reset();
    for (const Entity e : es) r.add(e, Position{1, 2});
    ZTH_EXPECT_EQ(stats::copies, 0ULL);
    ZTH_EXPECT_TRUE(stats::moves <= 400000ULL);
}
ZTH_TEST_TIMEOUT("fast", "removal is swap-and-pop: removing 50000 components moves each remaining one at most once", 10) {
    SparseSet<Position> s;
    for (EntityId id = 0; id < 50000; ++id) s.emplace(id, Position{1, 1});
    stats::reset();
    for (EntityId id = 0; id < 50000; ++id) ZTH_EXPECT_TRUE(s.erase(id));
    ZTH_EXPECT_EQ(s.size(), std::size_t{0});
    ZTH_EXPECT_EQ(stats::copies, 0ULL);
    ZTH_EXPECT_TRUE(stats::moves <= 50000ULL);  // shifting the array down would be about 1.25 billion moves
}
ZTH_TEST_TIMEOUT("fast", "each walks the smallest store: 3000 queries for a rare component among 200000 common ones", 10) {
    Registry r;
    for (int i = 0; i < 200000; ++i) {
        const Entity e = r.create();
        r.add(e, Position{static_cast<float>(i), 0});
        if (i % 66667 == 0) r.add(e, Frozen{});
    }
    ZTH_EXPECT_EQ(r.count<Frozen>(), std::size_t{3});
    int hits = 0;
    for (int q = 0; q < 3000; ++q) {
        r.each<Position, Frozen>([&](Entity, Position&, Frozen&) { ++hits; });
        r.each<Frozen, Position>([&](Entity, Frozen&, Position&) { ++hits; });
    }
    ZTH_EXPECT_EQ(hits, 18000);
}
ZTH_TEST_TIMEOUT("fast", "each hands out references: not one component is copied or moved (100000 entities, 3 passes)", 10) {
    Registry r;
    for (int i = 0; i < 100000; ++i) {
        const Entity e = r.create();
        r.add(e, Position{1, 1});
        r.add(e, Velocity{1, 1});
    }
    stats::reset();
    for (int pass = 0; pass < 3; ++pass) {
        r.each<Position, Velocity>([](Entity, Position& p, Velocity& v) {
            p.x += v.dx;
            p.y += v.dy;
        });
    }
    ZTH_EXPECT_EQ(stats::copies, 0ULL);
    ZTH_EXPECT_EQ(stats::moves, 0ULL);
}
ZTH_TEST_TIMEOUT("fast", "no allocation per entity or per component: 100000 entities with 2 components need fewer than 200 allocations", 10) {
    Registry r;
    zth::mem::Scope scope;
    for (int i = 0; i < 100000; ++i) {
        const Entity e = r.create();
        r.add(e, Position{});
        r.add(e, Velocity{});
    }
    ZTH_EXPECT_ALLOCS_AT_MOST(scope, 200);
}
ZTH_TEST_TIMEOUT("fast", "asking about a component type nobody has allocates nothing (no store is created by get / remove / each)", 10) {
    Registry r;
    const Entity e = r.create();
    r.add(e, Position{});
    zth::mem::Scope scope;
    ZTH_EXPECT_TRUE(r.get<Frozen>(e) == nullptr);
    ZTH_EXPECT_FALSE(r.has<Frozen>(e));
    ZTH_EXPECT_FALSE(r.remove<Frozen>(e));
    ZTH_EXPECT_EQ(r.count<Frozen>(), std::size_t{0});
    int n = 0;
    r.each<Frozen>([&](Entity, Frozen&) { ++n; });
    r.each<Position, Frozen>([&](Entity, Position&, Frozen&) { ++n; });
    ZTH_EXPECT_EQ(n, 0);
    ZTH_EXPECT_ALLOCS_AT_MOST(scope, 0);
}
ZTH_TEST_TIMEOUT("fast", "create / destroy cycles allocate nothing once the slots exist", 10) {
    Registry r;
    std::vector<Entity> es;
    for (int i = 0; i < 1000; ++i) {
        es.push_back(r.create());
        r.add(es.back(), Health{});
    }
    for (const Entity e : es) r.destroy(e);
    zth::mem::Scope scope;
    for (int round = 0; round < 50; ++round) {
        es.clear();
        for (int i = 0; i < 1000; ++i) es.push_back(r.create());
        for (const Entity e : es) r.destroy(e);
    }
    ZTH_EXPECT_ALLOCS_AT_MOST(scope, 10);
}

// ================================================ Clean =======================================================
ZTH_TEST("clean", "no raw new/delete/malloc/free (std::make_unique and the containers own the memory)") {
    ZTH_EXPECT_NO_RAW_MEMORY("sparse_set.hpp");
    ZTH_EXPECT_NO_RAW_MEMORY("registry.hpp");
}
ZTH_TEST("clean", "the sparse set is made of arrays: no std::map / std::set / std::list / hash tables in sparse_set.hpp") {
    const std::string code = ::zth::strip_comments_and_literals(::zth::read_file("sparse_set.hpp"));
    for (const char* word : {"map", "set", "list", "unordered_map", "unordered_set", "deque", "multimap"}) {
        if (std::string(word) == "set") continue;  // SparseSet itself
        ZTH_EXPECT_TRUE(::zth::find_word(code, word).empty());
    }
    ZTH_EXPECT_TRUE(code.find("std::set") == std::string::npos);
    ZTH_EXPECT_TRUE(code.find("#include <map>") == std::string::npos && code.find("#include <set>") == std::string::npos);
}
ZTH_TEST("clean", "each takes any callable (a template parameter), not std::function; no exceptions, no goto, no dynamic_cast") {
    for (const char* file : {"sparse_set.hpp", "registry.hpp"}) {
        const std::string code = ::zth::strip_comments_and_literals(::zth::read_file(file));
        for (const char* word : {"function", "throw", "try", "catch", "goto", "dynamic_cast", "any", "shared_ptr"}) {
            if (!::zth::find_word(code, word).empty()) {
                ::zth::fail(std::string(file) + " uses `" + word + "`");
                return;
            }
        }
    }
}
