# Fast path: what's different in C++

- **Struct of arrays.** A `std::vector<Position>` is contiguous, so a loop over it prefetches perfectly; a `std::vector<std::unique_ptr<Entity>>` or a class hierarchy with `virtual update()` jumps around the heap. Module 10 measured this with cache lines; ECS is the architecture built on that result.
- **Swap and pop** (`v[i] = std::move(v.back()); v.pop_back();`) is the standard way to delete from an unordered `std::vector` in O(1); the cost is that element order changes, which is why handles must not be indices into the dense array.
- **Generational indices** replace pointers and `shared_ptr`/`weak_ptr` in games: a handle is 8 bytes, copying it is free, and "is it still alive?" is one comparison. Rust's `slotmap`, Bevy's `Entity` and Unity's `Entity` are the same idea.
- **Type erasure with a base class.** The registry stores `Store<Position>`, `Store<Health>`... behind `StoreBase*` so it can call `erase(id)` on all of them. The templates give compile-time types for the hot loops; the virtual call is only on the rare path (`destroy`).
- **Variadic templates and fold expressions** (`(... && ...)`) are how `each<A, B, C>` is written once for any number of components; Python and Java have no equivalent that stays zero-cost.
- **`[[no_unique_address]]`** lets an empty member (the counter) take no space; the same trick makes a tag component such as `Frozen` free.
