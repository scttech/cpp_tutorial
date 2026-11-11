# Fast path: what's different in C++

- **Measure before you tune, and count before you time.** Wall-clock time depends on the machine; operation counts (comparisons, copies, allocations) do not. This site grades speed with counters and with *scaling tests* (the cost at `n` and at `2n`).
- **Big-O first**: an algorithm change (nested loops to sort plus two pointers, or a hash set) beats any micro-tuning. `std::sort` is `O(n log n)` comparisons; `std::unordered_set` is `O(1)` per lookup on average and needs a hash.
- **Sort a copy, not the caller's data** (`std::vector<T> sorted = values;`) when the parameter is `const`. Runs of equal values need care: count them, do not step through them one by one.
- **Instrumented types** (a class whose `operator==`, `operator<` and copy constructor bump a counter) make any generic algorithm measurable without changing it.
