# Fast path: what's different in C++

- **Allocation and construction are separate.** `new T` does both; in C++ you can do them apart. `StaticVector<T, N>` keeps `alignas(T) unsigned char storage_[sizeof(T) * N]`: raw bytes, **no `T` exists yet**. `std::array<T, N>` would construct all `N` elements up front (and need a default constructor).
- **Placement new** constructs an object at an address you choose: `::new (static_cast<void*>(address)) T(args...)`. It allocates nothing. The object's lifetime begins here.
- **Destroying is explicit too**: `std::destroy_at(p)` (or `p->~T()`) runs the destructor and ends the lifetime; the bytes stay. If you never destroy an element, its destructor never runs (a `shared_ptr` is never released, a string's memory leaks).
- **A slot is either a live `T` or raw memory.** Use **copy/move construction** (`emplace_back`) to put an object INTO a raw slot, and **assignment** (`a[i] = std::move(b)`) only between two LIVE objects. Assigning into a raw slot is undefined behavior: assignment operators assume the target is a valid object (a string would try to free garbage).
- **`insert(i, v)`**: move-construct the last element into the first raw slot (a new object), shift the rest up by move-assignment (from the back), then move-assign the value into the gap. **`erase(i)`**: shift down by move-assignment, then destroy the last (moved-from) element.
- **`std::launder`** tells the compiler "an object created by placement new really lives at this address" when you cast the bytes back to `T*`.
- **Rule of five, by hand**: copy = copy-construct each element; move = move-construct each element, then clear the source (a moved-from vector is empty and its moved-from elements are destroyed); both assignments guard against self-assignment and `clear()` first; the destructor `clear()`s.
- **Perfect forwarding**: `emplace_back(Args&&...)` with `T(std::forward<Args>(args)...)` builds in place; `push_back` calls it. No temporaries, no moves.
- This is how `std::inplace_vector` (C++26), `boost::static_vector`, `llvm::SmallVector` (inline part) and every `std::optional` work.
