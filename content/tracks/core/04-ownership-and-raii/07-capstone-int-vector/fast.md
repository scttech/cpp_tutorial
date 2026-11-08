# Fast path: what's different in C++

Model: `int* data_` (null when there is no block), `size_t size_`, `size_t capacity_` with `size_ <= capacity_`.

- **Rule of five** by hand: `delete[]` in the destructor; copy = new block of `size` + `std::copy` (capacity == size); copy assignment = copy then `swap`; moves `std::exchange` all three members (source: 0, 0, null), `noexcept`.
- **Growth**: when full, `capacity = capacity ? 2 * capacity : 1`; allocate, copy, release (amortized O(1) per push_back; 1000 push_backs = 11 allocations). `reserve(n)` = exact `n` if it grows; `shrink_to_fit()` = capacity becomes size; `clear()` keeps the block.
- **Aliasing**: `push_back(const int&)`, `insert(pos, const int&)` and `resize(n, const int&)` may be handed an element of the vector itself. Copy the value into a local (or write it to the new block) **before** releasing/shifting the old one.
- `insert`: shift with `std::copy_backward` and write after; `erase`: `std::copy` down. `at`/`front`/`back`/`pop_back`/`insert`/`erase` throw `std::out_of_range`.
- **Braces vs parentheses**: `IntVector(3, 7)` = three sevens (count constructor); `IntVector{3, 7}` = the list `3, 7` (initializer_list constructor wins for braces).
- No `malloc`, no standard container inside; `new[]` with `delete[]`; `new int[0]` is an allocation, so use `nullptr`.
