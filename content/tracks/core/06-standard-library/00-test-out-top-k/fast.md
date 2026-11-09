# Fast path: what's different in C++

- **Count with `std::unordered_map<K, size_t>`**: `++counts[key]` inserts a zero if the key is new. (`operator[]` INSERTS: use `find` or `count` to only look.) `std::map` works too (sorted, O(log n)).
- **`std::string_view` is a pointer and a length**, a view into text someone else owns. As a map key it avoids a heap allocation per word, but the text must outlive the map (lower-case ONE copy of the text and keep it alive).
- **Pick the top k with `std::partial_sort`** (O(n log k)), not a full sort of everything.
- **A comparator must be a strict weak ordering**: here `count` descending, then `word` ascending, written as one function that returns `a.second != b.second ? a.second > b.second : a.first < b.first`.
- **`std::tolower` / `std::isalpha` take an `int` that must be representable as `unsigned char` (or EOF)**: a plain `char` above 127 is negative on most platforms, which is undefined behavior. Cast: `static_cast<unsigned char>(c)`.
- **Return by value**; `result.reserve(k)` and `emplace_back(string_view)` build each string once.
