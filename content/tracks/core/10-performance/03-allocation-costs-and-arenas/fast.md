# Fast path: what's different in C++

- **You decide how memory is managed.** `new`/`delete` call the global allocator; containers take an allocator; C++17 `std::pmr` makes it a run-time choice. Nothing is garbage collected: an arena is the language's way to say "all of this dies together".
- **`std::pmr::monotonic_buffer_resource`** is the standard arena (give it a stack buffer to avoid the heap entirely); `std::pmr::unsynchronized_pool_resource` is a pool allocator. Use `std::pmr::polymorphic_allocator` for your own classes.
- **Containers must not outlive the resource**, and a `pmr` container copied into another resource does not share it: `a = b` keeps `a`'s resource.
- **Arenas and destructors**: an arena frees memory, it does not destroy objects. Use trivially destructible types, or call the destructors yourself before `reset()`.
- **Measure allocations** with a counting `operator new` or a counting resource, as the tests do here; a profiler (`perf`, Valgrind's `massif`/`callgrind`) shows the same for real programs.
