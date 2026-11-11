# Fast path: what's different in C++

- **You choose the layout.** C++ puts members in the order you wrote them, with padding for alignment (`sizeof`, `alignof`, `offsetof`); there is no hidden object header. Reorder members, split hot and cold data, use SoA: the compiler will not do it for you.
- **`std::vector` is the default container** because contiguous memory is what the cache and the prefetcher like. `std::list`, `std::map` and `std::unordered_map` allocate a node per element; walking them is pointer chasing. A `std::vector<std::unique_ptr<T>>` has the same problem.
- **Cache line size**: 64 bytes on x86-64 and most ARM cores (128 on some Apple cores). `std::hardware_destructive_interference_size` (in `<new>`) is the portable name for "keep these apart"; `std::hardware_constructive_interference_size` for "keep these together".
- **False sharing**: `alignas(64)` per-thread counters; `std::atomic` counters in an array are the classic victim.
- **Compilers can vectorize** a loop over contiguous floats (SIMD: 4 to 16 floats per instruction), which is one more reason SoA wins for number crunching.
