# Capstone 2: StaticVector&lt;T, N&gt;

The `RingBuffer` stored its elements in a `std::array<T, N>`: all `N` objects exist from the moment the buffer is created, and the elements "removed" from it are still there, moved-from. That is simple, and it costs two things: `T` must be **default constructible**, and an empty `RingBuffer<BigThing, 1000>` constructs a thousand objects.

`StaticVector<T, N>` is the same idea done properly: a vector with a fixed capacity and **no heap**, in which **an element exists only while it is in the vector**. Creating the vector creates no `T`. Adding an element constructs one, removing one destroys it. This is the technique behind `std::optional`, `std::variant`, `std::inplace_vector` (C++26) and every small-buffer container, and it is the last piece of the C++ object model this track has been building to: **allocating memory and creating an object are two separate things**.

## Raw storage and lifetimes

```cpp
alignas(T) unsigned char storage_[sizeof(T) * N];   // room for N objects, but NO object yet
std::size_t size_ = 0;                              // how many have been constructed
```

```
slot:    0     1     2     3     4         storage_ (raw bytes, aligned for T)
        [T]   [T]   [T]   ....  ....       size_ = 3
         alive alive alive  raw   raw
```

| To... | Write | Because |
|---|---|---|
| create an element in slot `i` | `::new (static_cast<void*>(storage_ + i * sizeof(T))) T(args...)` | **placement new**: runs the constructor at that address, allocates nothing |
| destroy the element in slot `i` | `std::destroy_at(ptr(i))` | runs the destructor, ends the lifetime, leaves the bytes |
| use the element in slot `i` | `*ptr(i)` | `ptr` casts the bytes to `T*` with `std::launder` |

**The rule that makes everything else follow:** a slot is *either* a live `T` *or* raw memory. You may **construct** into raw memory, you may **assign** between live objects, and you may **destroy** a live object. You may never assign into raw memory (a `std::string`'s assignment would try to free the garbage it finds there), and you may never forget to destroy what you constructed (the destructor of a `shared_ptr` is what releases the object).

## The rule of five, by hand

Because the class manages lifetimes, the compiler-generated copy and move would be wrong (they would copy the raw bytes). So all five special members are written:

- **destructor**: destroy every live element (`clear()`);
- **copy constructor**: copy-*construct* each element of the other vector into the raw slots;
- **move constructor**: move-*construct* each element, then `clear()` the source: a moved-from vector is empty;
- **copy / move assignment**: guard against `v = v`, `clear()` what you hold, then do the same as the constructors.

## The interface

| Member | What it does |
|---|---|
| `capacity()`, `size()`, `empty()`, `full()` | |
| `push_back(const T&)`, `push_back(T&&)`, `emplace_back(args...)` | add at the end (`false` when full); `emplace_back` builds in place |
| `pop_back()`, `clear()` | remove (destroying) the last / all |
| `insert(index, value)`, `erase(index)` | shift the elements; `false` for an out-of-range index (or a full vector, for `insert`) |
| `operator[]`, `front()`, `back()`, `data()`, `begin()`, `end()` | access; the iterators are plain pointers (the elements are contiguous) |
| `==` | same size and equal elements |

**`insert` and `erase` are the interesting ones**, because of the rule above. To insert in front of `[a, b, c]` (capacity 6):

1. the last element `c` must be moved into the first *raw* slot: that is a **construction**, a new object, `emplace_back(std::move(c))`;
2. then `b` moves into where `c` was, `a` into where `b` was: those slots hold live objects, so **assignment** is right;
3. finally the new value is move-assigned into the gap.

`erase` is the mirror image: shift down by assignment, then destroy the last element (which now holds a moved-from object).

## Tiers

- **Works**: all the operations, strings, a type with no default constructor, the algorithms.
- **Robust**: a random model check against `std::vector`, `shared_ptr` use counts (is every element released when it should be?), move-only types, self-assignment, an object-counting type that proves **every constructed object is destroyed exactly once**, alignment.
- **Fast**: an empty `StaticVector<T, 100>` constructs **no** `T`; `emplace_back` is one construction (no copy, no move); `erase` and `insert` use exactly the moves described above; copy and move constructors do one operation per element; **no heap allocation**; the object is `N` elements plus one counter.
- **Clean**: no `malloc`/`free`/`delete`, no `new T` (only placement new), no standard container: this class is the container.

## Hints for the order of work

1. `emplace_back`, `pop_back`, `clear`, the destructor, the size functions, `ptr`-based access: the first tests (and the lifetime-counting ones) pass.
2. The special members.
3. `insert` and `erase` last. Draw the slots on paper, with the raw ones crossed out.
