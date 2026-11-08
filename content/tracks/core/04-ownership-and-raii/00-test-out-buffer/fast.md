# Fast path: what's different in C++

You own raw memory, so write the five special members, once, carefully:

- **destructor** `delete[] data_;` | **copy ctor**: new block + `std::copy` | **copy assignment**: `if (this != &o) { Buffer copy(o); swap(copy); }` (strong guarantee) | **move ctor**: `std::exchange` the pointer and size, source left empty | **move assignment**: `delete[] data_;` then exchange, with a self-move guard. Moves are **`noexcept`**.
- An empty `Buffer` owns **no** block (null pointer), so moving and `Buffer(0)` never allocate.
- `swap` = exchange members (`noexcept`) + a `friend swap` for ADL.
- `resize`: new block, copy `min(n, size)`, zero the rest, release the old one. No allocation if the size is unchanged.
- `at` throws `std::out_of_range`; `operator[]` does not check. `begin()`/`end()` return pointers.
- Run it under the sanitizers: every mistake here is a precise report (double free, heap-buffer-overflow, leak, use-after-free).
