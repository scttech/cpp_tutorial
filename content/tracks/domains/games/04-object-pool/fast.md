# Fast path: what's different in C++

- **Object lifetime is separate from storage.** Allocating bytes does not create an object. `std::construct_at` (C++20; before that, placement `new`) starts a lifetime; `std::destroy_at` ends it. This separation is what allows `std::vector`'s spare capacity, `std::optional`, `std::variant` and your pool. Garbage-collected languages have no equivalent.
- **`alignas(T)` on the storage.** A `char buf[sizeof(T)]` is only 1-byte aligned; constructing a `double` or an `alignas(32)` SIMD type there is undefined behaviour and crashes on some CPUs. `alignas` on the slot type fixes it.
- **`std::launder`** tells the compiler "there is a new object at this address now" when you cast raw bytes to `T*` after construction. Forgetting it usually works at `-O0` and fails under optimisation.
- **Perfect forwarding** (`Args&&...` + `std::forward`) is how `emplace_back`, `make_unique` and `std::construct_at` itself pass constructor arguments with no extra copies. Python's `*args` copies references; C++ preserves value category.
- **Generational handles instead of pointers.** A pointer to a pooled object dangles silently when the slot is reused; a handle detects it. The same idea secures slot maps, ECS entities (the previous challenge) and file descriptors in operating systems.
- **Pools in real engines:** Quake and Doom's fixed entity arrays, Unity's DOTS chunks, `std::pmr::unsynchronized_pool_resource`, and the kernel's slab allocator.
