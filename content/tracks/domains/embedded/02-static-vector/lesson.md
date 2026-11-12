# Fixed-Capacity Vector

> **You're a firmware engineer.** The microcontroller has 64 KB of RAM, no operating system and a rule from the safety team: **no heap after start-up**, because `malloc` can fail, fragment, or take an unpredictable time. But you still want `push_back`, `erase` and range-for. `std::vector` allocates. `std::array` has no `size()` and default-constructs every slot. So you build a `StaticVector<T, N>`: up to `N` elements stored *inside* the object, on the stack or in a static, with the same lifetime rules as a real container.

## The core idea: storage is not objects

`alignas(T) std::byte storage_[N * sizeof(T)];` is just **bytes**. No `T` exists in it yet. You create an object in a slot with `std::construct_at(ptr, args...)` (placement new) and end its life with `std::destroy_at(ptr)`. Everything else follows from keeping track of which slots hold *living* objects (the first `size_`):

- **Never construct unused slots.** A `StaticVector<Sensor, 1000>` holding 3 sensors constructs exactly 3 `Sensor`s. This is why it works for types with no default constructor, and why an empty vector costs nothing.
- **Destroy what you remove.** `pop_back`, `erase`, `clear` and the destructor must call the destructor of every element that goes away, exactly once; otherwise a `std::string` element leaks its heap buffer and a lock guard never unlocks.
- **Align the bytes.** Without `alignas(T)` the array can start at any address; an over-aligned or `double` element then sits at a misaligned address (a crash on some CPUs, slow on others).
- **Reach a slot** with `std::launder(reinterpret_cast<T*>(storage_) + i)`: the cast turns bytes into a pointer, `launder` tells the compiler "a `T` object really lives there".

## Shifting elements correctly

`erase(i)` and `insert(i, v)` move the elements after `i`. Do it with **move assignment into live objects** (`*slot(k) = std::move(*slot(k + 1))`), not copies and never `memcpy`: an element may own memory or point to itself, and only its own move operation knows how to relocate it. The one slot that changes from *dead* to *alive* in `insert` (the new last one) must be **constructed**, not assigned; the one that goes from alive to dead in `erase` is **destroyed** by `pop_back`.

`swap_remove(i)` is the embedded idiom for "order does not matter": move the last element into the hole and shrink. One move instead of `size - i`.

## The rule of five, without a heap

The compiler's defaults would copy the *bytes* of `storage_`, which is wrong for any non-trivial `T` (two vectors would then destroy the same strings). So:

- copy constructor / copy assignment: copy element by element (assignment: `clear()` first; guard against `a = a`, which would otherwise destroy the data it is about to copy);
- move constructor / move assignment: move the elements and leave the source **empty** (documented);
- destructor: `clear()`.

## The tiers

- **Works**: construction counts (none for an empty vector), in-place `emplace_back`, `push_back` copy versus move, full vector, `pop_back`, indexing and iteration, `insert`/`erase`/`swap_remove` results, `clear`, copy and move (with self-assignment and no leaked object), move-only and no-default-constructor element types, alignment.
- **Robust**: 20000 random operations compared with `std::vector<int>`, tracking the number of live objects and checking that no element was relocated by raw bytes; nested vectors.
- **Fast**: zero allocations, size equal to the storage plus a counter, exactly `size - index` moves for `erase`/`insert` and at most one move for `swap_remove`, and construction counts that prove unused slots stay raw.
- **Clean**: no `new`/`delete`/`malloc`, no `memcpy`, no standard containers, `construct_at`/`destroy_at` and `alignas` present.
