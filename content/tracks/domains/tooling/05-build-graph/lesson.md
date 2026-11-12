# A Build Dependency Graph

> **You're on the developer-tools team.** `make`, `ninja`, Bazel, CMake, Gradle, `cargo`, `npm run`, CI pipelines and package managers all do the same three things with a graph of things that depend on other things: put them in an order where everything is built after what it needs; refuse (clearly) when that is impossible; and work out the smallest set that has to be rebuilt after a change. The code is short and the details are where build systems earn their reputation: a cycle error that says "cycle detected" without saying where, builds that do the same work in a different order on every run, a 200000-file monorepo that overflows the stack, a header change that silently skips a dependent.

## The graph

Each target lists its dependencies. A target with none is a **source file**: it exists, you do not build it. Everything is checked when the graph is made, in a fixed order: duplicate names, then dependencies that were never declared (a typo), then cycles. After that, every operation can assume a **DAG**, a directed acyclic graph.

## Topological order, deterministically

**Kahn's algorithm**: count each target's unmet dependencies; targets with none are *ready*; take a ready target, emit it, and decrement the count of everything that depends on it. A plain queue would give an order that depends on hash-map iteration, so builds would differ between runs and platforms. The fix is a rule people can predict: among the ready targets, the one **declared first** goes next. A priority queue keyed by declaration index does it in `O((V + E) log V)`.

## Cycles: report the cycle, not the fact

"Circular dependency" is useless in a build with 3000 targets. The depth-first search colours nodes *unvisited*, *on the current path*, *finished*; reaching a node that is on the path closes a cycle, and the path from that node to here *is* the cycle. Print it as `a -> b -> c -> a`. Two traps: a finished node reached again is a normal diamond (`top` depends on `l` and `r`, both on `base`), not a cycle; and the recursive version of this search overflows the stack on a long chain, so use an explicit stack of `(node, next dependency index)` frames.

## What to rebuild

`make`'s rule, which you implement: walk the needed targets in build order. A target is rebuilt if

1. its output does not exist, or
2. **any dependency is itself being rebuilt** in this run (its output is about to change, so the old timestamps mean nothing), or
3. any dependency has a modification time **strictly newer** than its own.

Rule 2 is the one that makes incremental builds correct, and the one people forget when they only compare timestamps: after `math.cpp` changes, `libm.a` has an old timestamp that may look newer than the *old* `math.o`, yet it must be rebuilt. Equal times count as up to date (file systems with 1-second timestamps would otherwise rebuild everything every time). A missing *source* is an error, found before anything is built.

## Parallelism

`levels` groups the buildable targets into rounds: round 1 needs only sources, round *k* needs something from round *k - 1*. Everything in a round can run at the same time, and the number of rounds is the **critical path**, the lower bound on build time with unlimited cores. `dependents` answers "what does this header affect?", which is what a file watcher or an IDE needs.

## The tiers

- **Works**: validation precedence, exact cycles, diamonds, deterministic order, `order_for`, `levels`, `dependents`, and every rule of `plan`.
- **Robust**: 300 random DAGs against naive references for order, levels and dependents; 1000 random timestamp scenarios against a make simulation (plus the idempotence property); planted cycles; odd names.
- **Fast**: a 200000-target chain (and a 200000-target cycle) without recursion; 100000 targets with 500000 edges.
- **Clean**: no raw memory or exceptions; no `std::function`, no sorting of the whole graph.
