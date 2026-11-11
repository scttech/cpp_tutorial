# Fast path: what's different in C++

- **Copies are explicit and invisible at the same time.** `auto x = big;`, `for (auto x : v)`, a by-value parameter and `a = a + b` all copy; a `const&` does not. Read your loop headers and parameter lists with that question in mind.
- **`reserve()`** for a vector or string whose final size you know; `emplace_back` and move for the rest. `std::vector(first, last)` builds a container from a range in one allocation.
- **Containers have costs per operation**: vector insert/erase at the front `O(n)`, `std::map` find `O(log n)`, `std::unordered_map` find `O(1)` average, `std::list` iteration is pointer chasing. `std::deque` or a reversed read can replace "insert at the front".
- **`find` once**: keep the iterator (or use `try_emplace`, `insert_or_assign`, `operator[]` deliberately) instead of searching twice.
- **Tooling**: `clang-tidy` has performance checks (`performance-for-range-copy`, `performance-unnecessary-value-param`, `performance-inefficient-string-concatenation`, `performance-inefficient-vector-operation`) that find several of these automatically (Module 8).
