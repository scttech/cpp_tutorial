# Fast path: what's different in C++

- **Alignment is a property of types.** `alignof(std::max_align_t)` is 16 on most platforms: that is what `malloc` promises, and what a `long double` or SSE vector may need. Returning a 8-aligned pointer is undefined behaviour waiting for the first `movaps`. `Heap::at<T>` checks `alignof(T)` for exactly this reason.
- **`reinterpret_cast` and strict aliasing.** Treating raw bytes as a `Header` is how every allocator works, and the standard only blesses it for `std::byte`/`char` arrays with `std::launder` or placement `new`. Real allocators confine the cast to one tiny, audited place (here `Heap::at`) so that the rest of the code is ordinary typed C++.
- **`operator new` is an allocator front end.** A `std::allocator`, `std::pmr::memory_resource` (Module 10), `tcmalloc` or `jemalloc` all do what you just did, with more size classes, thread caches and OS pages (`mmap`) instead of a fixed buffer.
- **Why `std::vector` is forbidden here:** an allocator that calls `new` calls itself. The bookkeeping lives in the memory being managed.
- **Boundary tags** (Knuth, *TAOCP* vol. 1, §2.5) trade 8 bytes per block for O(1) coalescing. dlmalloc stores the previous size only when the previous block is free, in its last 8 bytes; you can do that as an optimisation.
- **Detecting misuse is a design choice.** `free()` of a bad pointer is undefined behaviour in C, and glibc aborts with "free(): double free detected". Hardened allocators (Scudo, hardened_malloc) add canaries and quarantine freed memory, as AddressSanitizer does.
