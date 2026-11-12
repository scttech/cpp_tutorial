// MUTANT: unknown goals are skipped silently
#pragma once
// YOUR BUILD GRAPH (namespace build). Targets and what they depend on; a target with no dependencies is a SOURCE file (it exists on disk, it is never rebuilt).
//
// struct Target { std::string name; std::vector<std::string> deps; }
// enum class ErrorKind { DuplicateTarget, UnknownDependency, Cycle, MissingSource, UnknownGoal };
// struct Error { ErrorKind kind; std::string target; std::string detail; std::vector<std::string> cycle; }
//     DuplicateTarget:   target = the name declared twice
//     UnknownDependency: target = the target that names it, detail = the name that was never declared
//     Cycle:             cycle = the names of a cycle, the first name repeated at the end: {"a", "b", "c", "a"}; target = cycle.front()
//     MissingSource:     target = the source that does not exist
//     UnknownGoal:       target = the goal that is not a target
//
// Graph::make(targets) -> GraphResult { optional<Graph> graph; optional<Error> error; }
//     Declaration order matters: it is the tie-break everywhere below. Checks, in this order, reporting the FIRST problem of the first failing kind:
//       1. a name declared twice (the second declaration);   2. a dependency that is not a declared target (scan targets in declaration order, dependencies in listed order);
//       3. a cycle (a self-dependency "a: a" is the cycle {"a","a"}). Cycle search: depth-first from the targets in declaration order, dependencies in listed order, with an EXPLICIT stack (no recursion);
//          the first back edge found closes the cycle: it is the part of the current path from the node that was reached again up to the current node, then that node once more.
//          Example: x: a / a: b / b: a gives {"a","b","a"} (x is not part of the cycle).
//     Listing a dependency twice is harmless (it counts once). A Graph that exists is always acyclic and complete.
// size(), contains(name), deps(name) -> vector<string> (dependencies without duplicates, in listed order; empty for unknown names)
// order() -> vector<string>               ALL targets, every target after all its dependencies. Among the targets that are ready (all dependencies already listed) the one DECLARED FIRST goes next.
// order_for(goals) -> OrderResult { vector<string> order; optional<Error> error }     only the goals and everything they depend on, with the same rules. A goal that is not a target: UnknownGoal.
// levels(goals) -> vector<vector<string>>   the targets that need BUILDING (not the sources) reachable from the goals, grouped into rounds that can run in parallel: round 1 holds the targets whose dependencies
//                                           are all sources, round k those whose deepest dependency chain through non-sources has length k - 1. Inside a round: declaration order. Unknown goals are ignored. The number of rounds is the
//                                           critical path.
// dependents(name) -> vector<string>      everything that depends on `name`, directly or not (what a change to it can affect), in order() order, without `name` itself. Empty for unknown names.
// plan(goals, mtimes) -> PlanResult { vector<string> rebuild; optional<Error> error }     what `make` would rebuild, in build order. mtimes: unordered_map<string, int64_t> (modification times; a missing key means the
//     file does not exist). Only the goals and what they need are considered. Walking in order_for() order:
//       * a source with no mtime: MissingSource error (the first one in that order);
//       * a non-source is REBUILT if it has no mtime, or if ANY dependency is being rebuilt in this plan, or if any dependency has an mtime strictly NEWER than its own (equal is up to date).
//     UnknownGoal is reported like in order_for().
//
// All of it in time O(V + E) (O((V + E) log V) where a priority queue is used); a chain of 200000 targets must not overflow the stack.
#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <queue>
#include <string>
#include <unordered_map>
#include <vector>

namespace build {

struct Target {
    std::string name;
    std::vector<std::string> deps;
};

enum class ErrorKind { DuplicateTarget, UnknownDependency, Cycle, MissingSource, UnknownGoal };

struct Error {
    ErrorKind kind;
    std::string target;
    std::string detail;
    std::vector<std::string> cycle;
};

struct OrderResult {
    std::vector<std::string> order;
    std::optional<Error> error;
};

struct PlanResult {
    std::vector<std::string> rebuild;
    std::optional<Error> error;
};

class Graph;
struct GraphResult;

class Graph {
public:
    static GraphResult make(std::vector<Target> targets);

    std::size_t size() const { return names_.size(); }
    bool contains(const std::string& name) const { return index_.count(name) != 0; }
    std::vector<std::string> deps(const std::string& name) const {
        std::vector<std::string> out;
        const auto it = index_.find(name);
        if (it == index_.end()) return out;
        for (std::size_t d : deps_[it->second]) out.push_back(names_[d]);
        return out;
    }

    std::vector<std::string> order() const {
        std::vector<std::size_t> all(names_.size());
        for (std::size_t i = 0; i < all.size(); ++i) all[i] = i;
        return names(kahn(std::vector<char>(names_.size(), 1)));
    }

    OrderResult order_for(const std::vector<std::string>& goals) const {
        std::vector<char> in;
        if (auto e = reachable(goals, in)) return OrderResult{{}, *e};
        return OrderResult{names(kahn(in)), std::nullopt};
    }

    std::vector<std::vector<std::string>> levels(const std::vector<std::string>& goals) const {
        std::vector<std::string> known;
        for (const std::string& g : goals)
            if (contains(g)) known.push_back(g);
        std::vector<char> in;
        reachable(known, in);
        const std::vector<std::size_t> topo = kahn(in);
        std::vector<std::size_t> level(names_.size(), 0);
        std::size_t deepest = 0;
        for (std::size_t v : topo) {
            if (deps_[v].empty()) continue;
            std::size_t l = 1;
            for (std::size_t d : deps_[v]) l = std::max(l, level[d] + 1);
            level[v] = l;
            deepest = std::max(deepest, l);
        }
        std::vector<std::vector<std::string>> rounds(deepest);
        for (std::size_t v = 0; v < names_.size(); ++v)   // declaration order inside a round
            if (in[v] && !deps_[v].empty()) rounds[level[v] - 1].push_back(names_[v]);
        return rounds;
    }

    std::vector<std::string> dependents(const std::string& name) const {
        std::vector<std::string> out;
        const auto it = index_.find(name);
        if (it == index_.end()) return out;
        std::vector<char> hit(names_.size(), 0);
        std::vector<std::size_t> stack = {it->second};
        while (!stack.empty()) {
            const std::size_t v = stack.back();
            stack.pop_back();
            for (std::size_t r : rdeps_[v])
                if (!hit[r]) {
                    hit[r] = 1;
                    stack.push_back(r);
                }
        }
        for (std::size_t v : kahn(std::vector<char>(names_.size(), 1)))
            if (hit[v]) out.push_back(names_[v]);
        return out;
    }

    PlanResult plan(const std::vector<std::string>& goals, const std::unordered_map<std::string, std::int64_t>& mtimes) const {
        std::vector<char> in;
        if (auto e = reachable(goals, in)) return PlanResult{{}, *e};
        std::vector<char> rebuilt(names_.size(), 0), has(names_.size(), 0);
        std::vector<std::int64_t> mt(names_.size(), 0);
        PlanResult result;
        for (std::size_t v : kahn(in)) {
            const auto it = mtimes.find(names_[v]);
            has[v] = it != mtimes.end();
            if (has[v]) mt[v] = it->second;
            if (deps_[v].empty()) {   // a source file
                if (!has[v]) return PlanResult{{}, Error{ErrorKind::MissingSource, names_[v], "", {}}};
                continue;
            }
            bool need = !has[v];
            for (std::size_t d : deps_[v])
                if (rebuilt[d] || (has[v] && mt[d] > mt[v])) need = true;
            if (need) {
                rebuilt[v] = 1;
                result.rebuild.push_back(names_[v]);
            }
        }
        return result;
    }

private:
    std::vector<std::string> names(const std::vector<std::size_t>& ids) const {
        std::vector<std::string> out;
        out.reserve(ids.size());
        for (std::size_t i : ids) out.push_back(names_[i]);
        return out;
    }

    // the members of `in`, dependencies first, the earliest declared of the ready ones next
    std::vector<std::size_t> kahn(const std::vector<char>& in) const {
        std::vector<std::size_t> remaining(names_.size(), 0), out;
        std::priority_queue<std::size_t, std::vector<std::size_t>, std::greater<std::size_t>> ready;
        for (std::size_t v = 0; v < names_.size(); ++v) {
            if (!in[v]) continue;
            remaining[v] = deps_[v].size();
            if (remaining[v] == 0) ready.push(v);
        }
        while (!ready.empty()) {
            const std::size_t v = ready.top();
            ready.pop();
            out.push_back(v);
            for (std::size_t r : rdeps_[v])
                if (in[r] && --remaining[r] == 0) ready.push(r);
        }
        return out;
    }

    // marks the goals and their dependencies in `in`; UnknownGoal for a goal that is not a target
    std::optional<Error> reachable(const std::vector<std::string>& goals, std::vector<char>& in) const {
        in.assign(names_.size(), 0);
        std::vector<std::size_t> stack;
        for (const std::string& g : goals) {
            const auto it = index_.find(g);
            if (it == index_.end()) continue;
            if (!in[it->second]) {
                in[it->second] = 1;
                stack.push_back(it->second);
            }
        }
        while (!stack.empty()) {
            const std::size_t v = stack.back();
            stack.pop_back();
            for (std::size_t d : deps_[v])
                if (!in[d]) {
                    in[d] = 1;
                    stack.push_back(d);
                }
        }
        return std::nullopt;
    }

    std::vector<std::string> names_;
    std::unordered_map<std::string, std::size_t> index_;
    std::vector<std::vector<std::size_t>> deps_, rdeps_;
};

struct GraphResult {
    std::optional<Graph> graph;
    std::optional<Error> error;
};

inline GraphResult Graph::make(std::vector<Target> targets) {
    Graph g;
    const auto fail = [](Error e) { return GraphResult{std::nullopt, std::move(e)}; };
    for (const Target& t : targets) {
        if (!g.index_.emplace(t.name, g.names_.size()).second) return fail({ErrorKind::DuplicateTarget, t.name, "", {}});
        g.names_.push_back(t.name);
    }
    const std::size_t n = g.names_.size();
    g.deps_.assign(n, {});
    g.rdeps_.assign(n, {});
    std::vector<std::size_t> stamp(n, static_cast<std::size_t>(-1));
    for (std::size_t i = 0; i < n; ++i)
        for (const std::string& d : targets[i].deps) {
            const auto it = g.index_.find(d);
            if (it == g.index_.end()) return fail({ErrorKind::UnknownDependency, g.names_[i], d, {}});
            if (stamp[it->second] == i) continue;   // listed twice
            stamp[it->second] = i;
            g.deps_[i].push_back(it->second);
            g.rdeps_[it->second].push_back(i);
        }
    // depth-first search with an explicit stack: 0 = unvisited, 1 = on the current path, 2 = finished
    std::vector<char> color(n, 0);
    std::vector<std::size_t> position(n, 0);
    struct Frame {
        std::size_t node, next;
    };
    for (std::size_t start = 0; start < n; ++start) {
        if (color[start]) continue;
        std::vector<Frame> stack = {{start, 0}};
        color[start] = 1;
        position[start] = 0;
        while (!stack.empty()) {
            Frame& f = stack.back();
            if (f.next == g.deps_[f.node].size()) {
                color[f.node] = 2;
                stack.pop_back();
                continue;
            }
            const std::size_t w = g.deps_[f.node][f.next++];
            if (color[w] == 1) {
                Error e{ErrorKind::Cycle, g.names_[w], "", {}};
                for (std::size_t k = position[w]; k < stack.size(); ++k) e.cycle.push_back(g.names_[stack[k].node]);
                e.cycle.push_back(g.names_[w]);
                return fail(std::move(e));
            }
            if (color[w] == 0) {
                color[w] = 1;
                position[w] = stack.size();
                stack.push_back({w, 0});
            }
        }
    }
    return GraphResult{std::move(g), std::nullopt};
}

}  // namespace build
