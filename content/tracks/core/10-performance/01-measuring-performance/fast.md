# Fast path: what's different in C++

- **Profile, then optimize.** Compile with optimization (`-O2`) for any real measurement: an unoptimized build has different hot spots. Repeat the measurement; look at the whole run, not one call.
- **Algorithm and data structure first.** `O(n²)` to `O(n log n)` is worth more than any micro-optimization. Know what you pay: `std::vector` search `O(n)`, `std::set`/`std::map` `O(log n)`, `std::unordered_*` `O(1)` average, `std::sort` `O(n log n)`.
- **Counters over clocks** for tests and for reasoning: comparisons, copies, allocations, bytes touched.
- **Amdahl's law**: speeding up a stage that is 5 % of the run time can never give more than a 5 % gain; speeding up the 90 % stage can.
- **Cheap habits that cost nothing**: pass big objects by `const&`, `reserve()` when you know the size, `emplace_back`, prefix `++` on iterators, and `'\n'` instead of `std::endl`. They are habits, not a replacement for measuring.
