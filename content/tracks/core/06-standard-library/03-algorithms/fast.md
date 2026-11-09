# Fast path: what's different in C++

- **Algorithms take iterator pairs and never resize the container.** `std::remove_if`/`std::remove`/`std::unique` shuffle the keepers to the front and return the new logical end; the **erase-remove idiom** finishes the job: `v.erase(std::remove_if(v.begin(), v.end(), pred), v.end());` (C++20: `std::erase_if(v, pred)`).
- **Erasing in a loop is O(n) per erase.** The idiom is one O(n) pass.
- **`std::sort` needs random access and a strict weak ordering** (`comp(a, a)` false). Use `<`/`>` and compare with `!=` before falling through to the next key; never `<=`. `std::stable_sort` keeps the order of equal elements; `std::partial_sort` and `std::nth_element` are cheaper when you need only the top k or the median.
- **`std::partition` vs `std::stable_partition`**: only the second preserves the relative order inside each group.
- **`std::accumulate(first, last, init, op)`**: the type of `init` is the type of the result: `0` gives integer arithmetic, `0.0` double. It folds left; `std::reduce` may reorder (needs an associative op).
- **`std::unique` removes only adjacent equal elements**: sort first.
- **Binary search** (`lower_bound`, `upper_bound`, `equal_range`, `binary_search`) requires the range to be partitioned with respect to the key (sorted), and uses only `operator<`. It is O(log n) comparisons on any iterator, O(log n) steps on random access ones.
- **`std::tolower`/`std::toupper`/`std::isalpha` take an `int` that must be an `unsigned char` value (or EOF)**: cast the `char`.
- **Prefer a named algorithm to a loop** (`find_if`, `count_if`, `all_of`, `transform`...): clearer, fewer off-by-one errors, and some have parallel overloads.
