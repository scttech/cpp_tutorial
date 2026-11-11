# Fast path: what's different in C++

- **`std::map` with a custom comparator** (`std::greater<Price>`) keeps bids best-first, so "the best price" is always `begin()`: O(1) to read, O(log n) to insert a new level. Exchanges use exactly this (or a sorted array / skip list / price-indexed array when prices are dense).
- **`std::list` iterators are stable.** Insert or erase elsewhere and your iterator is still valid. `std::vector` iterators are not: any `push_back` may invalidate them. That is why the order queue is a list and the index stores iterators. (Real engines go further: an intrusive list or a ring per level, to avoid a heap allocation per order.)
- **A secondary index** (`unordered_map<OrderId, Where>`) answers "where is order 12345?" without a scan: the classic way to keep both ordered-by-price and lookup-by-id fast. Databases call it a secondary index.
- **Templates to share code between the two sides.** A single `match(Order&, Book&, Result&)` serves both buy and sell books, and the compiler generates two optimised copies. In a language without templates you would duplicate the code or pay for a virtual call.
- **Determinism and replay.** Exchanges can replay the day's messages and must produce byte-identical trades. No hash-order iteration, no pointer-value ordering, no floating point: the same reasons the money type uses integers.
- **Latency matters more than throughput here**: allocation per order is the first thing real engines remove (pools, arenas, as in Modules 10 and the games challenges).
