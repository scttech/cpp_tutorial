# Fast path: what's different in C++

| Container | Good at | Bad at | Notes |
|---|---|---|---|
| `std::vector` | everything by default: contiguous, cache friendly, O(1) `push_back`/index | insert/erase in the middle or at the FRONT: O(n) | references/iterators invalidated by growth |
| `std::array` | fixed size, no heap | | size is part of the type |
| `std::deque` | O(1) at BOTH ends | slower indexing than vector; not contiguous | the default `std::queue` |
| `std::list` | O(1) insert/erase/**splice** anywhere you hold an iterator; iterators and addresses never invalidated | no indexing; cache unfriendly; a heap block per element | `splice` moves nodes without copying elements |
| `std::map` / `std::set` | sorted order, range queries, O(log n) | slower than hashing for plain lookup | **`m[key]` INSERTS a default value if missing**: use `find`/`count`/`contains` to look |
| `std::unordered_map` / `unordered_set` | O(1) average lookup | iteration order is unspecified (and changes); worst case O(n) with a bad hash | `reserve` before bulk inserts |
| `std::priority_queue` | the largest on top, O(log n) push/pop | no iteration, no search | a **max**-heap; use `std::greater<>` for a min-heap |

- **Choose by the operations you do most**, then measure. The default is `vector`; reach for another one when profiling or an operation count says so.
- **Complexity is visible**: a quadratic solution on 300000 elements does not fail, it just never finishes. The Fast tier here uses a time limit, an operation count and a comparison count to catch it.
- **Top-k**: a heap of size k is O(n log k); sorting everything is O(n log n). `std::partial_sort` and `std::nth_element` are the library versions.
