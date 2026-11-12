#define ZTH_TRACK_ALLOCATIONS
#include <algorithm>
#include <functional>
#include <set>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "build.hpp"
#include "zth_test.hpp"

using namespace build;
using Names = std::vector<std::string>;
using Times = std::unordered_map<std::string, std::int64_t>;

namespace {

GraphResult G(std::vector<Target> t) { return Graph::make(std::move(t)); }
Graph must(std::vector<Target> t) {
    GraphResult r = Graph::make(std::move(t));
    return r.graph ? *r.graph : *Graph::make({}).graph;
}
std::string join(const Names& n) {
    std::string s;
    for (const std::string& x : n) s += (s.empty() ? "" : ",") + x;
    return s;
}
std::string err(const GraphResult& r) {
    if (!r.error) return "OK";
    const char* k[] = {"DuplicateTarget", "UnknownDependency", "Cycle", "MissingSource", "UnknownGoal"};
    return std::string(k[static_cast<int>(r.error->kind)]) + " " + r.error->target + (r.error->detail.empty() ? "" : "/" + r.error->detail) + (r.error->cycle.empty() ? "" : " [" + join(r.error->cycle) + "]");
}

// The example used in several tests; declaration order: app, main.o, util.o, libm.a, math.o, then the sources
std::vector<Target> project() {
    return {{"app", {"main.o", "util.o", "libm.a"}}, {"main.o", {"main.cpp", "util.h"}}, {"util.o", {"util.cpp", "util.h"}}, {"libm.a", {"math.o"}}, {"math.o", {"math.cpp"}},
            {"main.cpp", {}}, {"util.cpp", {}}, {"util.h", {}}, {"math.cpp", {}}};
}
Times fresh() {
    return {{"main.cpp", 10}, {"util.cpp", 10}, {"util.h", 10}, {"math.cpp", 10}, {"main.o", 20}, {"util.o", 20}, {"math.o", 20}, {"libm.a", 21}, {"app", 30}};
}

// ---- random DAGs whose declaration order is unrelated to the dependency direction ------------------------------------------------------
struct RandomDag {
    std::vector<Target> targets;
};
RandomDag random_dag(zth::Lcg& rng, int n, int max_deps) {
    std::vector<int> rank(static_cast<std::size_t>(n));
    for (int i = 0; i < n; ++i) rank[static_cast<std::size_t>(i)] = i;
    for (int i = n - 1; i > 0; --i) std::swap(rank[static_cast<std::size_t>(i)], rank[static_cast<std::size_t>(rng.below(i + 1))]);
    // node i may depend on node j when rank[j] < rank[i]
    RandomDag d;
    for (int i = 0; i < n; ++i) {
        Target t{"t" + std::to_string(i), {}};
        for (int k = 0, m = rng.below(max_deps + 1); k < m; ++k) {
            const int j = rng.below(n);
            if (rank[static_cast<std::size_t>(j)] < rank[static_cast<std::size_t>(i)]) t.deps.push_back("t" + std::to_string(j));   // duplicates may happen
        }
        d.targets.push_back(t);
    }
    return d;
}

// ---- reference implementations, deliberately naive ---------------------------------------------------------------------------------------
Names ref_order(const std::vector<Target>& t, const std::set<std::string>* only = nullptr) {
    std::set<std::string> done;
    Names out;
    for (;;) {
        bool progressed = false;
        for (const Target& x : t) {
            if (done.count(x.name) || (only && !only->count(x.name))) continue;
            bool ready = true;
            for (const std::string& d : x.deps) ready = ready && done.count(d);
            if (!ready) continue;
            done.insert(x.name);
            out.push_back(x.name);
            progressed = true;
            break;   // always restart: the earliest declared ready target goes first
        }
        if (!progressed) return out;
    }
}
std::set<std::string> ref_reach(const std::vector<Target>& t, const Names& goals) {
    std::set<std::string> in;
    bool changed = true;
    for (const std::string& g : goals) in.insert(g);
    while (changed) {
        changed = false;
        for (const Target& x : t)
            if (in.count(x.name))
                for (const std::string& d : x.deps) changed |= in.insert(d).second;
    }
    return in;
}
const Target* find(const std::vector<Target>& t, const std::string& n) {
    for (const Target& x : t)
        if (x.name == n) return &x;
    return nullptr;
}
Names ref_plan(const std::vector<Target>& t, const Names& goals, const Times& times) {
    const auto reach = ref_reach(t, goals);
    std::unordered_set<std::string> rebuilt;
    Names out;
    for (const std::string& n : ref_order(t, &reach)) {
        const Target* x = find(t, n);
        if (x->deps.empty()) continue;
        const bool has = times.count(n) != 0;
        bool need = !has;
        for (const std::string& d : x->deps) {
            if (rebuilt.count(d)) need = true;
            else if (has && times.at(d) > times.at(n)) need = true;
        }
        if (need) {
            rebuilt.insert(n);
            out.push_back(n);
        }
    }
    return out;
}

}  // namespace

// ================================================ Works =======================================================
ZTH_TEST("works", "a valid graph: size, membership and dependencies without duplicates") {
    const GraphResult r = G(project());
    ZTH_EXPECT_TRUE(r.graph.has_value() && !r.error);
    ZTH_EXPECT_EQ(r.graph->size(), std::size_t{9});
    ZTH_EXPECT_TRUE(r.graph->contains("app") && r.graph->contains("math.cpp") && !r.graph->contains("nope"));
    ZTH_EXPECT_EQ(join(r.graph->deps("app")), std::string("main.o,util.o,libm.a"));
    ZTH_EXPECT_EQ(join(r.graph->deps("main.cpp")), std::string(""));
    ZTH_EXPECT_EQ(join(r.graph->deps("nope")), std::string(""));
    const Graph twice = must({{"a", {"b", "c", "b", "c", "b"}}, {"b", {}}, {"c", {}}});
    ZTH_EXPECT_EQ(join(twice.deps("a")), std::string("b,c"));
    ZTH_EXPECT_EQ(twice.size(), std::size_t{3});
    ZTH_EXPECT_TRUE(G({}).graph.has_value() && G({}).graph->size() == 0 && G({}).graph->order().empty());
    ZTH_EXPECT_TRUE(G({{"only", {}}}).graph.has_value());
}
ZTH_TEST("works", "duplicate targets and unknown dependencies, with the first culprit in declaration order") {
    ZTH_EXPECT_EQ(err(G({{"a", {}}, {"b", {}}, {"a", {}}})), std::string("DuplicateTarget a"));
    ZTH_EXPECT_EQ(err(G({{"a", {}}, {"b", {}}, {"c", {}}, {"b", {}}, {"a", {}}})), std::string("DuplicateTarget b"));
    ZTH_EXPECT_EQ(err(G({{"a", {"x"}}, {"b", {"y"}}})), std::string("UnknownDependency a/x"));
    ZTH_EXPECT_EQ(err(G({{"a", {"b", "x", "y"}}, {"b", {}}})), std::string("UnknownDependency a/x"));
    ZTH_EXPECT_EQ(err(G({{"a", {"b"}}, {"b", {"c"}}})), std::string("UnknownDependency b/c"));
    ZTH_EXPECT_EQ(err(G({{"a", {"x"}}, {"a", {}}})), std::string("DuplicateTarget a"));   // duplicates are checked before dependencies
    ZTH_EXPECT_EQ(err(G({{"a", {"x"}}, {"b", {"a"}}, {"b", {}}})), std::string("DuplicateTarget b"));
    ZTH_EXPECT_EQ(err(G({{"a", {"b"}}, {"b", {"a"}}, {"c", {"zzz"}}})), std::string("UnknownDependency c/zzz"));   // unknown names before cycles
}
ZTH_TEST("works", "cycles: the actual cycle is reported, found by depth-first search in declaration order") {
    ZTH_EXPECT_EQ(err(G({{"a", {"a"}}})), std::string("Cycle a [a,a]"));
    ZTH_EXPECT_EQ(err(G({{"a", {"b"}}, {"b", {"a"}}})), std::string("Cycle a [a,b,a]"));
    ZTH_EXPECT_EQ(err(G({{"b", {"a"}}, {"a", {"b"}}})), std::string("Cycle b [b,a,b]"));
    ZTH_EXPECT_EQ(err(G({{"a", {"b"}}, {"b", {"c"}}, {"c", {"a"}}, {"d", {"a"}}})), std::string("Cycle a [a,b,c,a]"));
    ZTH_EXPECT_EQ(err(G({{"x", {"a"}}, {"a", {"b"}}, {"b", {"a"}}})), std::string("Cycle a [a,b,a]"));   // x leads into the cycle but is not on it
    ZTH_EXPECT_EQ(err(G({{"x", {"y"}}, {"y", {"z"}}, {"z", {"b"}}, {"a", {"b"}}, {"b", {"c"}}, {"c", {"b"}}})), std::string("Cycle b [b,c,b]"));
    ZTH_EXPECT_EQ(err(G({{"a", {"b", "c"}}, {"b", {"d"}}, {"c", {"d"}}, {"d", {"b"}}})), std::string("Cycle b [b,d,b]"));   // reached through a, then b, then d, then back to b
    ZTH_EXPECT_EQ(err(G({{"p", {"q"}}, {"q", {}}, {"r", {"s"}}, {"s", {"r"}}, {"t", {"t"}}})), std::string("Cycle r [r,s,r]"));   // the first cycle found, not the shortest
    ZTH_EXPECT_EQ(err(G({{"a", {"b", "c"}}, {"b", {}}, {"c", {"a"}}})), std::string("Cycle a [a,c,a]"));   // a finished sibling (b) is not mistaken for a cycle
}
ZTH_TEST("works", "diamonds and shared dependencies are not cycles") {
    ZTH_EXPECT_TRUE(G({{"top", {"l", "r"}}, {"l", {"base"}}, {"r", {"base"}}, {"base", {}}}).graph.has_value());
    ZTH_EXPECT_TRUE(G({{"a", {"b", "c", "d"}}, {"b", {"c", "d"}}, {"c", {"d"}}, {"d", {}}}).graph.has_value());
}
ZTH_TEST("works", "order(): dependencies first, ties broken by declaration order") {
    const Graph g = must(project());
    // main.cpp (declared 6th) and the other sources are ready at the start; the earliest declared ready target goes next every time
    ZTH_EXPECT_EQ(join(g.order()), std::string("main.cpp,util.cpp,util.h,main.o,util.o,math.cpp,math.o,libm.a,app"));
    const Graph chain = must({{"c", {"b"}}, {"b", {"a"}}, {"a", {}}});
    ZTH_EXPECT_EQ(join(chain.order()), std::string("a,b,c"));
    const Graph flat = must({{"z", {}}, {"y", {}}, {"x", {}}});
    ZTH_EXPECT_EQ(join(flat.order()), std::string("z,y,x"));   // declaration order, not alphabetical
    const Graph mix = must({{"app", {"lib"}}, {"b", {}}, {"lib", {"a"}}, {"a", {}}});
    ZTH_EXPECT_EQ(join(mix.order()), std::string("b,a,lib,app"));
}
ZTH_TEST("works", "order_for(): only the goals and their dependencies; unknown goals are reported") {
    const Graph g = must(project());
    ZTH_EXPECT_EQ(join(g.order_for({"libm.a"}).order), std::string("math.cpp,math.o,libm.a"));
    ZTH_EXPECT_EQ(join(g.order_for({"main.o", "libm.a"}).order), std::string("main.cpp,util.h,main.o,math.cpp,math.o,libm.a"));
    ZTH_EXPECT_EQ(join(g.order_for({"util.h"}).order), std::string("util.h"));
    ZTH_EXPECT_TRUE(g.order_for({}).order.empty() && !g.order_for({}).error);
    ZTH_EXPECT_EQ(join(g.order_for({"app", "app", "main.o"}).order), join(g.order()));   // repeated goals count once
    const OrderResult bad = g.order_for({"app", "nope"});
    ZTH_EXPECT_TRUE(bad.error.has_value() && bad.error->kind == ErrorKind::UnknownGoal && bad.error->target == "nope" && bad.order.empty());
}
ZTH_TEST("works", "levels(): rounds of targets that can run in parallel; sources are not built") {
    const Graph g = must(project());
    const auto rounds = g.levels({"app"});
    ZTH_EXPECT_EQ(rounds.size(), std::size_t{3});
    ZTH_EXPECT_EQ(join(rounds[0]), std::string("main.o,util.o,math.o"));
    ZTH_EXPECT_EQ(join(rounds[1]), std::string("libm.a"));
    ZTH_EXPECT_EQ(join(rounds[2]), std::string("app"));
    const auto small = g.levels({"main.o"});
    ZTH_EXPECT_TRUE(small.size() == 1 && join(small[0]) == "main.o");
    ZTH_EXPECT_TRUE(g.levels({"main.cpp"}).empty());   // a source needs no building
    ZTH_EXPECT_TRUE(g.levels({}).empty());
    ZTH_EXPECT_TRUE(g.levels({"nope"}).empty());
    const auto mixed = g.levels({"nope", "libm.a"});   // unknown goals are skipped
    ZTH_EXPECT_TRUE(mixed.size() == 2 && join(mixed[0]) == "math.o" && join(mixed[1]) == "libm.a");
    const Graph skip = must({{"top", {"mid", "src"}}, {"mid", {"low"}}, {"low", {"src"}}, {"src", {}}});
    const auto r = skip.levels({"top"});
    ZTH_EXPECT_TRUE(r.size() == 3 && join(r[0]) == "low" && join(r[1]) == "mid" && join(r[2]) == "top");   // 'top' also depends on a source directly: it still waits for mid
}
ZTH_TEST("works", "dependents(): everything a change can affect, in build order, without the target itself") {
    const Graph g = must(project());
    ZTH_EXPECT_EQ(join(g.dependents("util.h")), std::string("main.o,util.o,app"));
    ZTH_EXPECT_EQ(join(g.dependents("math.cpp")), std::string("math.o,libm.a,app"));
    ZTH_EXPECT_EQ(join(g.dependents("app")), std::string(""));
    ZTH_EXPECT_EQ(join(g.dependents("nope")), std::string(""));
    ZTH_EXPECT_EQ(join(g.dependents("libm.a")), std::string("app"));
    const Graph diamond = must({{"top", {"l", "r"}}, {"l", {"base"}}, {"r", {"base"}}, {"base", {}}});
    ZTH_EXPECT_EQ(join(diamond.dependents("base")), std::string("l,r,top"));   // 'top' once, though two paths lead to it
}
ZTH_TEST("works", "plan(): nothing to do when every output is newer than its inputs; equal times are up to date") {
    const Graph g = must(project());
    const PlanResult r = g.plan({"app"}, fresh());
    ZTH_EXPECT_TRUE(!r.error && r.rebuild.empty());
    Times equal = fresh();
    for (auto& [name, t] : equal) t = 5;
    ZTH_EXPECT_TRUE(g.plan({"app"}, equal).rebuild.empty());
    Times older_by_one = fresh();
    older_by_one["libm.a"] = 9;   // older than math.o (20)
    ZTH_EXPECT_EQ(join(g.plan({"app"}, older_by_one).rebuild), std::string("libm.a,app"));
}
ZTH_TEST("works", "plan(): a changed source rebuilds everything above it, in build order, each target once") {
    const Graph g = must(project());
    Times t = fresh();
    t["util.h"] = 25;
    ZTH_EXPECT_EQ(join(g.plan({"app"}, t).rebuild), std::string("main.o,util.o,app"));
    t = fresh();
    t["math.cpp"] = 40;
    ZTH_EXPECT_EQ(join(g.plan({"app"}, t).rebuild), std::string("math.o,libm.a,app"));
    t = fresh();
    t["main.cpp"] = 100;
    t["util.cpp"] = 100;
    t["math.cpp"] = 100;
    ZTH_EXPECT_EQ(join(g.plan({"app"}, t).rebuild), std::string("main.o,util.o,math.o,libm.a,app"));
}
ZTH_TEST("works", "plan(): a rebuilt dependency forces its dependents even when their old timestamps look newer") {
    // app (30) looks newer than libm.a (21) and main.o (20), but math.cpp changed: math.o is rebuilt, so libm.a and app must be too
    const Graph g = must(project());
    Times t = fresh();
    t["math.cpp"] = 12;   // newer than nothing it feeds (math.o is 20): up to date
    ZTH_EXPECT_TRUE(g.plan({"app"}, t).rebuild.empty());
    t["math.cpp"] = 21;   // newer than math.o (20) only
    ZTH_EXPECT_EQ(join(g.plan({"app"}, t).rebuild), std::string("math.o,libm.a,app"));
    Times far = fresh();
    far["math.o"] = 5;    // an output OLDER than its source: rebuilt; then libm.a (21) and app (30) follow although both are newer than the old math.o
    ZTH_EXPECT_EQ(join(g.plan({"app"}, far).rebuild), std::string("math.o,libm.a,app"));
}
ZTH_TEST("works", "plan(): a missing output is rebuilt, and so is everything above it") {
    const Graph g = must(project());
    Times t = fresh();
    t.erase("math.o");
    ZTH_EXPECT_EQ(join(g.plan({"app"}, t).rebuild), std::string("math.o,libm.a,app"));
    t = fresh();
    t.erase("app");
    ZTH_EXPECT_EQ(join(g.plan({"app"}, t).rebuild), std::string("app"));
    t = fresh();
    t.erase("main.o");
    t.erase("util.o");
    ZTH_EXPECT_EQ(join(g.plan({"app"}, t).rebuild), std::string("main.o,util.o,app"));
    Times none = {{"main.cpp", 1}, {"util.cpp", 1}, {"util.h", 1}, {"math.cpp", 1}};
    ZTH_EXPECT_EQ(join(g.plan({"app"}, none).rebuild), std::string("main.o,util.o,math.o,libm.a,app"));
}
ZTH_TEST("works", "plan(): only what the goals need is looked at; unknown goals and missing sources are errors") {
    const Graph g = must(project());
    Times t = fresh();
    t["math.cpp"] = 99;
    ZTH_EXPECT_TRUE(g.plan({"main.o"}, t).rebuild.empty());   // math is not needed for main.o
    ZTH_EXPECT_EQ(join(g.plan({"libm.a"}, t).rebuild), std::string("math.o,libm.a"));
    ZTH_EXPECT_EQ(join(g.plan({"main.o", "libm.a"}, t).rebuild), std::string("math.o,libm.a"));
    const PlanResult bad = g.plan({"nope"}, t);
    ZTH_EXPECT_TRUE(bad.error.has_value() && bad.error->kind == ErrorKind::UnknownGoal && bad.error->target == "nope" && bad.rebuild.empty());
    Times missing = fresh();
    missing.erase("util.cpp");
    const PlanResult m = g.plan({"app"}, missing);
    ZTH_EXPECT_TRUE(m.error.has_value() && m.error->kind == ErrorKind::MissingSource && m.error->target == "util.cpp" && m.rebuild.empty());
    ZTH_EXPECT_TRUE(!g.plan({"libm.a"}, missing).error);   // util.cpp is not needed for libm.a
    Times two_missing = fresh();
    two_missing.erase("util.h");
    two_missing.erase("math.cpp");
    const PlanResult first = g.plan({"app"}, two_missing);
    ZTH_EXPECT_TRUE(first.error && first.error->target == "util.h");   // the first missing source in build order
    ZTH_EXPECT_TRUE(g.plan({}, {}).rebuild.empty() && !g.plan({}, {}).error);
}
ZTH_TEST("works", "plan(): a goal that is a source needs nothing; a source with an mtime of zero or less is still a file") {
    const Graph g = must(project());
    ZTH_EXPECT_TRUE(g.plan({"util.h"}, fresh()).rebuild.empty());
    Times t = fresh();
    t["main.cpp"] = -5;
    t["util.h"] = 0;
    ZTH_EXPECT_TRUE(g.plan({"app"}, t).rebuild.empty());   // older sources are fine
    ZTH_EXPECT_TRUE(g.plan({"app"}, {}).error.has_value());
}

// ================================================ Robust ======================================================
ZTH_TEST("robust", "300 random DAGs (declaration order unrelated to the dependency direction): order() equals the naive earliest-ready-first reference") {
    zth::Lcg rng(2024);
    for (int round = 0; round < 300; ++round) {
        const RandomDag d = random_dag(rng, 1 + rng.below(40), 4);
        const GraphResult r = G(d.targets);
        ZTH_EXPECT_TRUE(r.graph.has_value());
        if (!r.graph) return;
        const Names got = r.graph->order(), want = ref_order(d.targets);
        ZTH_EXPECT_EQ(got.size(), d.targets.size());
        ZTH_EXPECT_EQ(join(got), join(want));
        // and as a property: every dependency comes earlier
        std::unordered_map<std::string, std::size_t> pos;
        for (std::size_t i = 0; i < got.size(); ++i) pos[got[i]] = i;
        for (const Target& t : d.targets)
            for (const std::string& dep : t.deps) ZTH_EXPECT_TRUE(pos[dep] < pos[t.name]);
    }
}
ZTH_TEST("robust", "order_for, levels, dependents on random DAGs against naive references") {
    zth::Lcg rng(7);
    for (int round = 0; round < 300; ++round) {
        const int n = 2 + rng.below(30);
        const RandomDag d = random_dag(rng, n, 3);
        const Graph g = must(d.targets);
        Names goals;
        for (int k = 0, m = 1 + rng.below(3); k < m; ++k) goals.push_back("t" + std::to_string(rng.below(n)));
        const auto reach = ref_reach(d.targets, goals);
        ZTH_EXPECT_EQ(join(g.order_for(goals).order), join(ref_order(d.targets, &reach)));
        // levels: longest chain of non-sources
        std::unordered_map<std::string, int> lvl;
        std::function<int(const std::string&)> level = [&](const std::string& x) -> int {
            const auto it = lvl.find(x);
            if (it != lvl.end()) return it->second;
            const Target* t = find(d.targets, x);
            int l = 0;
            if (!t->deps.empty()) {
                l = 1;
                for (const std::string& dep : t->deps) l = std::max(l, level(dep) + 1);
            }
            return lvl[x] = l;
        };
        std::vector<Names> want(1);
        for (const Target& t : d.targets)
            if (reach.count(t.name) && !t.deps.empty()) {
                const std::size_t l = static_cast<std::size_t>(level(t.name));
                if (want.size() <= l) want.resize(l + 1);
                want[l].push_back(t.name);
            }
        want.erase(want.begin());
        const auto got = g.levels(goals);
        ZTH_EXPECT_EQ(got.size(), want.size());
        for (std::size_t i = 0; i < std::min(got.size(), want.size()); ++i) ZTH_EXPECT_EQ(join(got[i]), join(want[i]));
        // dependents of a random node: the naive transitive closure of "who lists me"
        const std::string x = "t" + std::to_string(rng.below(n));
        std::set<std::string> dep_set;
        bool changed = true;
        dep_set.insert(x);
        while (changed) {
            changed = false;
            for (const Target& t : d.targets) {
                if (dep_set.count(t.name)) continue;
                for (const std::string& dep : t.deps)
                    if (dep_set.count(dep)) {
                        dep_set.insert(t.name);
                        changed = true;
                        break;
                    }
            }
        }
        dep_set.erase(x);
        Names expected;
        for (const std::string& name : ref_order(d.targets))
            if (dep_set.count(name)) expected.push_back(name);
        ZTH_EXPECT_EQ(join(g.dependents(x)), join(expected));
    }
}
ZTH_TEST("robust", "plan() on random DAGs with random timestamps (some missing) against a naive make simulation") {
    zth::Lcg rng(31);
    int rebuilds = 0;
    for (int round = 0; round < 1000; ++round) {
        const int n = 2 + rng.below(25);
        const RandomDag d = random_dag(rng, n, 3);
        const Graph g = must(d.targets);
        Times t;
        for (const Target& x : d.targets) {
            if (!x.deps.empty() && rng.below(6) == 0) continue;   // output missing
            t[x.name] = rng.below(12);
        }
        Names goals = {"t" + std::to_string(rng.below(n))};
        if (rng.below(3) == 0) goals.push_back("t" + std::to_string(rng.below(n)));
        const PlanResult r = g.plan(goals, t);
        const Names want = ref_plan(d.targets, goals, t);
        ZTH_EXPECT_TRUE(!r.error);
        ZTH_EXPECT_EQ(join(r.rebuild), join(want));
        rebuilds += static_cast<int>(want.size());
        // property: rebuilding is idempotent -- give every rebuilt target a timestamp past all others and nothing remains to do
        Times after = t;
        // (a rebuilt target is newer than its dependencies only if no dependency was stamped later: stamp in build order)
        std::int64_t clock = 1000;
        for (const std::string& x : r.rebuild) after[x] = ++clock;
        ZTH_EXPECT_TRUE(g.plan(goals, after).rebuild.empty());
    }
    ZTH_EXPECT_TRUE(rebuilds > 500);
}
ZTH_TEST("robust", "cycles planted in random DAGs are found and reported as real cycles of the graph") {
    zth::Lcg rng(5);
    for (int round = 0; round < 500; ++round) {
        const int n = 3 + rng.below(25);
        RandomDag d = random_dag(rng, n, 3);
        // plant a back edge: make node a depend on a node that depends (transitively) on it, by adding a direct 2-cycle or longer loop
        const int a = rng.below(n), b = rng.below(n);
        d.targets[static_cast<std::size_t>(a)].deps.push_back("t" + std::to_string(b));
        d.targets[static_cast<std::size_t>(b)].deps.push_back("t" + std::to_string(a));
        const GraphResult r = G(d.targets);
        ZTH_EXPECT_TRUE(r.error.has_value() && r.error->kind == ErrorKind::Cycle);
        if (!r.error) continue;
        const Names& c = r.error->cycle;
        ZTH_EXPECT_TRUE(c.size() >= 2 && c.front() == c.back());
        for (std::size_t i = 0; i + 1 < c.size(); ++i) {   // every consecutive pair is an edge
            const Target* t = find(d.targets, c[i]);
            ZTH_EXPECT_TRUE(t != nullptr && std::find(t->deps.begin(), t->deps.end(), c[i + 1]) != t->deps.end());
        }
        std::set<std::string> distinct(c.begin(), c.end() - 1);
        ZTH_EXPECT_EQ(distinct.size(), c.size() - 1);   // a simple cycle: no repeated node inside
        ZTH_EXPECT_EQ(r.error->target, c.front());
    }
}
ZTH_TEST("robust", "names with odd characters, empty names, and long names are just names") {
    const Graph g = must({{"", {"a b"}}, {"a b", {"x/y", "\xC3\xA9"}}, {"x/y", {}}, {"\xC3\xA9", {}}, {std::string(100000, 'n'), {""}}});
    ZTH_EXPECT_EQ(g.size(), std::size_t{5});
    const Names first_order = g.order();
    ZTH_EXPECT_EQ(first_order.front(), std::string("x/y"));
    ZTH_EXPECT_TRUE(g.contains("") && g.contains(std::string(100000, 'n')));
    ZTH_EXPECT_EQ(join(g.dependents("x/y")), std::string("a b,,") + std::string(100000, 'n'));
}

// ================================================ Fast ========================================================
ZTH_TEST_TIMEOUT("fast", "a chain of 200000 targets: no recursion anywhere (make, order, levels, dependents, plan)", 20) {
    const int n = 200000;
    std::vector<Target> t;
    t.reserve(static_cast<std::size_t>(n));
    for (int i = n - 1; i >= 0; --i) t.push_back({"c" + std::to_string(i), i == 0 ? Names{} : Names{"c" + std::to_string(i - 1)}});
    const GraphResult r = G(std::move(t));
    ZTH_EXPECT_TRUE(r.graph.has_value());
    const Names order = r.graph->order();
    ZTH_EXPECT_TRUE(order.size() == static_cast<std::size_t>(n) && order.front() == "c0" && order.back() == "c199999");
    ZTH_EXPECT_EQ(r.graph->levels({"c199999"}).size(), static_cast<std::size_t>(n - 1));
    ZTH_EXPECT_EQ(r.graph->dependents("c0").size(), static_cast<std::size_t>(n - 1));
    Times times;
    for (int i = 0; i < n; ++i) times["c" + std::to_string(i)] = 10 + i;
    ZTH_EXPECT_TRUE(r.graph->plan({"c199999"}, times).rebuild.empty());
    times["c0"] = 1000000;
    ZTH_EXPECT_EQ(r.graph->plan({"c199999"}, times).rebuild.size(), static_cast<std::size_t>(n - 1));
    // and a 200000-long cycle is reported, not a stack overflow
    std::vector<Target> loop;
    for (int i = 0; i < n; ++i) loop.push_back({"l" + std::to_string(i), {"l" + std::to_string((i + 1) % n)}});
    const GraphResult bad = G(std::move(loop));
    ZTH_EXPECT_TRUE(bad.error.has_value() && bad.error->kind == ErrorKind::Cycle && bad.error->cycle.size() == static_cast<std::size_t>(n) + 1);
}
ZTH_TEST_TIMEOUT("fast", "100000 targets with 500000 edges: linearithmic time", 20) {
    zth::Lcg rng(1);
    const int n = 100000;
    std::vector<int> rank(static_cast<std::size_t>(n));
    for (int i = 0; i < n; ++i) rank[static_cast<std::size_t>(i)] = i;
    for (int i = n - 1; i > 0; --i) std::swap(rank[static_cast<std::size_t>(i)], rank[static_cast<std::size_t>(rng.below(i + 1))]);
    std::vector<int> by_rank(static_cast<std::size_t>(n));
    for (int i = 0; i < n; ++i) by_rank[static_cast<std::size_t>(rank[static_cast<std::size_t>(i)])] = i;
    std::vector<Target> t(static_cast<std::size_t>(n));
    for (int i = 0; i < n; ++i) {
        t[static_cast<std::size_t>(i)].name = "t" + std::to_string(i);
        const int r = rank[static_cast<std::size_t>(i)];
        if (r == 0) continue;
        for (int k = 0; k < 5; ++k) t[static_cast<std::size_t>(i)].deps.push_back("t" + std::to_string(by_rank[static_cast<std::size_t>(rng.below(r))]));
    }
    const GraphResult g = G(std::move(t));
    ZTH_EXPECT_TRUE(g.graph.has_value());
    const Names order = g.graph->order();
    ZTH_EXPECT_EQ(order.size(), static_cast<std::size_t>(n));
    const std::string last = "t" + std::to_string(by_rank[static_cast<std::size_t>(n - 1)]);
    ZTH_EXPECT_TRUE(!g.graph->levels({last}).empty());
    Times times;
    for (const std::string& x : order) times[x] = 1;
    ZTH_EXPECT_TRUE(g.graph->plan({last}, times).rebuild.empty());
    const std::string touched = g.graph->order_for({last}).order.front();   // a source that the goal really needs
    times[touched] = 5;
    ZTH_EXPECT_TRUE(!g.graph->plan({last}, times).rebuild.empty());
    ZTH_EXPECT_TRUE(!g.graph->dependents(touched).empty());
}
ZTH_TEST("fast", "queries on a small graph allocate a bounded amount") {
    const Graph g = must(project());
    const Times t = fresh();
    zth::mem::Scope scope;
    std::size_t total = 0;
    for (int i = 0; i < 200; ++i) total += g.order().size() + g.plan({"app"}, t).rebuild.size() + g.levels({"app"}).size() + g.dependents("util.h").size();
    ZTH_EXPECT_ALLOCS_AT_MOST(scope, 200 * 80);
    ZTH_EXPECT_TRUE(total > 0);
}

// ================================================ Clean =======================================================
ZTH_TEST("clean", "no raw memory, no exceptions, no goto") {
    ZTH_EXPECT_NO_RAW_MEMORY("build.hpp");
    const std::string code = ::zth::strip_comments_and_literals(::zth::read_file("build.hpp"));
    for (const char* word : {"throw", "try", "catch", "goto"}) ZTH_EXPECT_TRUE(::zth::find_word(code, word).empty());
}
ZTH_TEST("clean", "no recursion to hide behind std::function, and no sorting of the whole graph to get an order") {
    const std::string code = ::zth::strip_comments_and_literals(::zth::read_file("build.hpp"));
    for (const char* word : {"function", "sort", "stable_sort", "boost", "graph_traits"}) ZTH_EXPECT_TRUE(::zth::find_word(code, word).empty());
}
