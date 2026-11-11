# Fast path: what's different in C++

- **Views instead of copies.** `std::string_view` and `std::span` refer to memory somebody else owns. They make parsing allocation-free, but they dangle if the owner dies or changes: a `string_view` into a temporary string is a bug.
- **`std::from_chars` / `std::to_chars`** (C++17): number conversion without locale, exceptions or allocation. Check `ec` AND `ptr` (did it consume the whole text?).
- **Containers**: `std::unordered_map` for lookup by key (needs a hash), `std::map` for ordered iteration. `reserve()` avoids rehashing. Keys are moved in when you pass a temporary.
- **Top-k** does not need a full sort: keep a small sorted array, a `std::priority_queue`, or use `std::partial_sort`/`std::nth_element`.
- **Measure what you change.** Counters and budgets (as in these tests) catch regressions in CI the way unit tests catch bugs; a profiler tells you what to work on next.
