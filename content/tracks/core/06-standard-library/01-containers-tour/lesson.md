# A tour of the containers

The standard library has a dozen containers, and choosing between them is a daily C++ decision. The wrong choice rarely gives a wrong *answer*; it gives a slow program, or a pile of needless copies. This lesson is five small jobs, each with a container that fits it. The tests check the results, and the **Fast** tier checks the *cost*.

## The jobs

| # | Function | The right tool | The trap |
|---|---|---|---|
| 1 | `word_counts`, `count_of` | `std::map` (sorted keys) | `counts[w]` **inserts** a zero when `w` is missing. Fine when counting, a bug when only *looking*: use `find`. |
| 2 | `dedupe` | `std::unordered_set` | searching a `vector` for every value is O(n) each, O(n²) in all: 300000 values never finish |
| 3 | `process_queue` | `std::deque` | `vector::erase(begin())` shifts every element: O(n) per pop |
| 4 | `move_to_front` | `std::list::splice` | copying elements to reorder them; `splice` relinks one node, touches no element, and keeps every iterator valid |
| 5 | `k_smallest` | `std::priority_queue` | sorting everything to keep ten values: O(n log n) instead of O(n log k) |

## Reading the table of costs

| Container | insert/erase at the end | at the front | in the middle | lookup by value | order |
|---|---|---|---|---|---|
| `vector` | O(1) amortized | O(n) | O(n) | O(n) | insertion |
| `deque` | O(1) | O(1) | O(n) | O(n) | insertion |
| `list` | O(1) | O(1) | O(1) with an iterator | O(n) | insertion |
| `map`, `set` | O(log n) | | | O(log n) | sorted |
| `unordered_map`, `unordered_set` | O(1) average | | | O(1) average | unspecified |
| `priority_queue` | push O(log n) | | | top O(1) | largest first |

A few things to remember:

- **`vector` is the default.** Contiguous memory is fast because of the CPU cache, so a `vector` often beats a theoretically better container for small n. Switch when an operation count or a profiler says so.
- **`std::map::operator[]` inserts.** `if (m[key] == 0)` creates the entry. To look without inserting: `find`, `count`, `contains` (C++20), or `at` (throws if missing).
- **`unordered_*` iteration order is unspecified** and can change after an insert. Never depend on it; copy into a `vector` and sort when the order matters.
- **Iterator invalidation** differs per container: a `vector` insert may invalidate everything; a `list` never invalidates an iterator except to an erased element; a `map` insert never invalidates anything.
- **`std::priority_queue` is a MAX-heap**: `top()` is the largest. For the `k` smallest you keep the *worst* of the best `k` on top, so you know what to throw out.

## Your task

Fill in `containers.hpp` (every function is a stub). The tiers: **Works** (the results), **Robust** (edge cases: empty input, k larger than n, an iterator that must stay valid, a missing word), **Fast** (300000 values deduplicated, 100000 jobs queued, zero element copies for `move_to_front`, and few comparisons for `k_smallest`).
