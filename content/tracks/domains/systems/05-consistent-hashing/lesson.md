# Consistent Hashing Ring

> **You're an engineer on a systems team** running a cache cluster. Each key must live on exactly one server, and every client must agree which. The textbook answer, `server = hash(key) % N`, has a fatal flaw: when `N` changes (a server joins, dies, or is replaced) *almost every key* maps to a different server. The whole cache goes cold at once, and the database behind it gets hit by every request. **Consistent hashing** fixes that: when a server joins or leaves, only about `1/N` of the keys move.

## The idea: a circle

Hash both servers and keys onto the same circle of 64-bit numbers. A key belongs to the **first server point clockwise** from the key's position (wrapping from the highest point to the lowest). Removing a server only affects the keys that were pointing at it (they fall through to the next point); adding one only steals the keys between it and its predecessor. Nothing moves *between* surviving servers.

## Virtual nodes

One point per server gives wildly uneven arcs. So each server puts **many points** on the ring (here `vnodes_per_weight`, default 100, at `hash64("<name>#<i>")`). The arcs average out: the standard deviation of a server's share falls like `1/sqrt(vnodes)`. A **weight** `w` simply means `w` times as many points, hence `w` times as many keys: use it for bigger machines.

## The data structure

A `std::vector<Point{hash, node}>` kept **sorted**. Lookup is `std::lower_bound` (binary search), wrapped to the start if it falls off the end: O(log P), and it allocates nothing. Why a sorted vector and not a `std::map`? Contiguous memory is much friendlier to the cache, and the ring changes rarely but is read constantly.

- **Add a node:** build its points, sort *them* (a few hundred), append, then **`std::inplace_merge`**: O(P) instead of re-sorting everything with O(P log P).
- **Remove a node:** `std::remove_if` + `erase`; it is *stable*, so the vector stays sorted. (`std::partition` would scramble the order.)
- **Node table:** `std::unordered_map<std::string, weight>`; its keys never move, so each `Point` can hold a `const std::string*` and `lookup` can return it with no copying.
- **Ties:** order points by `(hash, node name)` so that the ring does not depend on the order in which nodes were added.

## Replicas

`lookup_n(key, n)` walks clockwise from the key's position and collects the first `n` **distinct** nodes (many consecutive points belong to the same node, so you must skip repeats). This is how Cassandra, Dynamo and Riak choose the servers for `n` copies of a value.

## The tiers

- **Works**: an independent `std::map`-based reference, wrap-around, weights, removal, replicas, order independence, pointer stability.
- **Robust**: adding a node moves keys *only to it* (about 5% for 20 servers), removing one moves only its own keys, balance across 10 servers within a factor of about 2, the 1:2:3 weights, replica sets that survive unrelated removals, and 400 random operations compared with a from-scratch reference.
- **Fast**: zero allocations per lookup (even for long keys), 2 million lookups on 100000 points, 1500 `add_node` calls (merge, not sort), mass removal.
- **Clean**: no raw memory or exceptions, no `map`/`set`/`list` for the points, a `lower_bound` binary search, and hashing only through `hash64`.
