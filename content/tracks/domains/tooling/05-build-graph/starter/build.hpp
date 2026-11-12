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
    static GraphResult make(std::vector<Target> targets);   // TODO
    std::size_t size() const { return 0; }
    bool contains(const std::string&) const { return false; }
    std::vector<std::string> deps(const std::string&) const { return {}; }
    std::vector<std::string> order() const { return {}; }
    OrderResult order_for(const std::vector<std::string>&) const { return {}; }
    std::vector<std::vector<std::string>> levels(const std::vector<std::string>&) const { return {}; }
    std::vector<std::string> dependents(const std::string&) const { return {}; }
    PlanResult plan(const std::vector<std::string>&, const std::unordered_map<std::string, std::int64_t>&) const { return {}; }
};

struct GraphResult {
    std::optional<Graph> graph;
    std::optional<Error> error;
};

inline GraphResult Graph::make(std::vector<Target>) { return GraphResult{Graph(), std::nullopt}; }

}  // namespace build
