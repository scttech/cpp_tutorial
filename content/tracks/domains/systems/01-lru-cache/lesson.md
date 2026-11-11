# LRU Cache

> **You're an engineer on a systems team.** A service reads configuration records, user profiles or database pages that are expensive to fetch, so you keep the most recent ones in memory. Memory is bounded, so when the cache is full something has to go. **LRU** (least recently used) throws away the entry that has gone unused the longest, on the bet that what you used lately you will use again soon. The cache sits on the hot path: every operation must be **O(1)**.

## The classic design

Two data structures, each covering the other's weakness:

- a **doubly linked list** ordered by recency (front = most recent): cheap to move a node to the front, cheap to drop the back, but finding a key takes O(n);
- a **hash map from key to list node**: finds any entry in O(1), but has no order.

Together: look up the node through the map, **relink** it to the front of the list, and drop `back()` (and its map entry) when over capacity.

## The C++ detail that makes it elegant: `std::list::splice`

`items_.splice(items_.begin(), items_, it)` moves one node inside the same list **by relinking pointers**: nothing is copied, moved or allocated, and **iterators and pointers to the node stay valid**. That is why the map can store `std::list::iterator`s for the life of an entry, and why `get()` can safely return a `V*`. (A `std::vector` or an erase-and-reinsert would invalidate or move the data.)

## The API

`get` (hit = promote + count), `peek`/`contains` (look without touching), `put` (insert or update + promote; returns whether it **evicted**), `erase`, `clear`, `size`, `capacity`, `hits`, `misses`, `for_each` (most recent first). Capacity **0** is legal and stores nothing.

## Details that decide correctness

- **Updating an existing key is not an eviction.** In a full cache, `put(existing, v)` must not throw anything away.
- **Evict from the back, and erase the map entry too.** Remove the node from *both* structures or the map is left pointing at freed memory.
- **`peek` and `contains` do not promote** (they are `const`): a monitoring thread that merely looks must not change what gets evicted.
- **One lookup per operation.** `if (contains(k)) { find(k) }` hashes twice. Do `auto it = index_.find(k)` once and use `it`.

## The tiers

- **Works**: hits and misses, eviction order, promotion by `get` and by an update, `peek`/`contains` leave the order alone, `erase`, `clear`, capacity 0 and 1, string keys, move-only values.
- **Robust**: a pointer from `get` stays valid, 30000 random operations against a slow reference model (put/get/erase/contains/clear, capacities 0 to 17, order checked as it goes), long streams.
- **Fast**: *counted* keys prove it: one hash and about one comparison per `get`; at most three hashes and one key copy per `put`, no value copies; updates and gets allocate nothing; a new entry costs two nodes; and 400000 operations on 30000 entries finish within the timeout.
- **Clean**: no raw memory or exceptions, no `find_if`/`count_if`-style scanning, and `splice` is how you promote.
