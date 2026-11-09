# The algorithms library

`<algorithm>` and `<numeric>` hold about a hundred functions for the things programs do with sequences: search, count, copy, transform, sort, partition, merge, reduce. They are tested, fast, and they say what you mean. **Prefer a named algorithm to a hand-written loop**: `std::find_if` tells the reader you are searching, a `for` loop makes them work it out.

This lesson is a data-cleaning pipeline over a `std::vector<Record>`, one algorithm at a time.

| Step | Function | Algorithm | The trap |
|---|---|---|---|
| 1 | `remove_invalid` | `std::remove_if` + `erase` | `remove_if` does **not** shrink the vector: it moves the keepers to the front and returns the new end. Without `erase`, the tail is garbage. Erasing in a loop is O(n) each, O(n²) in all |
| 2 | `normalize_names` | `std::transform` | `std::tolower`/`std::toupper` need an `unsigned char` value (a negative `char` is undefined behavior) |
| 3 | `sort_records` | `std::sort` + comparator | the comparator must be a **strict weak ordering**: `a.score > b.score`, never `>=`. `std::sort` is not stable |
| 4 | `partition_adults` | `std::stable_partition` | `std::partition` does not preserve the order inside each group |
| 5 | `average_score` | `std::accumulate` | the initial value decides the type of the sum: `0` is an `int`, use `0.0` |
| 6 | `unique_names_sorted` | `transform`, `sort`, `unique`, `erase` | `std::unique` removes only **adjacent** duplicates (sort first), and like `remove` it returns the new end |
| 7 | `find_by_name` | `std::find_if` | compare the result with `end()` before dereferencing |
| 8 | `count_between` | `std::lower_bound` | binary search needs **sorted** input; `lower_bound` is the first element `>= value` |

## Things to remember

- **Algorithms work on iterator pairs** `[first, last)` and never change the size of a container: that is why `remove` and `unique` return a new end and you `erase` the tail (the "erase-remove idiom"). C++20 has `std::erase_if(v, pred)` that does both in one call.
- **Comparators**: a *strict weak ordering* means `comp(a, a)` is false, and if `comp(a, b)` then not `comp(b, a)`. A comparator with `<=` or `>=` breaks that and makes `std::sort` read out of bounds or crash, usually only on larger inputs.
- **Searching a sorted range**: `lower_bound` (first `>= x`), `upper_bound` (first `> x`), `equal_range` and `binary_search` take O(log n) steps (on random access iterators). On an unsorted range their result is meaningless.
- **Lambdas are the usual predicates and comparators**: `[](const Record& r) { return r.age >= 18; }`.
- **`<numeric>`**: `std::accumulate`, `std::reduce` (parallel-friendly), `std::transform_reduce`, `std::iota`, `std::partial_sum`, `std::gcd`.
- **Algorithms with a predicate in the name** (`find_if`, `count_if`, `remove_if`, `all_of`, `any_of`, `none_of`) cover most loops.

## Your task

Fill in `cleaning.hpp`. The tiers: **Works** (the pipeline), **Robust** (empty input, many equal records, stability, bytes above 127, bounds outside the data), **Fast** (200000 records through the erase-remove idiom, `count_between` with about `2 * log2(n)` comparisons, a hundred thousand records sorted and partitioned).
