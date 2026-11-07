# `std::vector`

`std::vector<T>` is the container you will reach for first: a **resizable array**. It owns its elements, stores them
contiguously on the heap, grows as you add to it, and cleans up after itself.

```cpp
#include <vector>
std::vector<int> v;          // empty
v.push_back(10);             // {10}
v.push_back(20);             // {10, 20}
v.size();                    // 2
v[1];                        // 20  : unchecked, like a C array
v.at(1);                     // 20  : checked (throws std::out_of_range)
for (int x : v) { ... }      // range-for
```

## Size vs capacity

- **size** is how many elements the vector holds.
- **capacity** is how many it can hold before it must allocate a bigger block.

When `push_back` finds the block full, the vector allocates a larger one (typically double), **moves every element
across**, and frees the old block. That is why `push_back` is *amortized* constant time, and also why a loop that
pushes 1000 elements causes about 10 allocations. If you know the final size, tell the vector up front:

```cpp
std::vector<int> v;
v.reserve(1000);   // capacity >= 1000, size still 0: one allocation, no growth later
v.resize(1000);    // size == 1000: creates 1000 elements (zeros)  <-- a different thing!
```

`reserve` never changes what the vector *contains*; `resize` does. Mixing them up is a very common bug.

## Iterators and invalidation

An iterator (or a pointer or a reference) into a vector points at a *spot in its block*. If the block moves, or an
element is erased, those spots can go stale:

- **Growing past the capacity** reallocates: every iterator, pointer and reference into the vector is now **dangling**.
- **Erasing** an element invalidates iterators at and after it.

So this is a bug:

```cpp
for (auto it = v.begin(); it != v.end(); ++it) {
    if (*it % 2 == 0) v.erase(it);   // `it` is invalid afterwards; ++it is undefined behavior
}
```

The idiom that works is *erase-remove*: `std::remove_if` shuffles the elements you keep to the front, then one
`erase` cuts off the tail. C++20 wraps both in one call: `std::erase_if(v, predicate)`.

## Two more things to know

- **`vector<bool>` is not a normal vector.** It packs bits and hands out proxy objects instead of real
  references. Prefer `vector<char>` or `std::bitset` when you need booleans.
- **`v[i]` vs `v.at(i)`**: `[]` is as unchecked as a C array (undefined behavior out of range); `at()` checks and
  throws. Use `[]` in hot loops where the index is known good and `at()` when it is not.

## Run-length encoding

Your exercise is a tiny compressor: `{5, 5, 5, 2, 2, 9}` becomes the runs `{5 x3, 2 x2, 9 x1}`. `rle.hpp` (read-only)
defines `Run { int value; int count; }`.

## Your task

Implement in `main.cpp` (the starter's `main` shows how the capacity grows; run it first):

| Function | Behavior |
|---|---|
| `std::vector<Run> encode(const std::vector<int>&)` | Run-length encode. Only *adjacent* equal values merge. |
| `std::vector<int> decode(const std::vector<Run>&)` | The inverse. A run whose count is zero or negative contributes nothing. |
| `std::vector<long long> squares(int n)` | `0, 1, 4, ...` for `0..n-1`; empty if `n <= 0`. Watch for overflow. |
| `std::vector<int> make_buffer(std::size_t n)` | An *empty* vector with capacity for at least `n`. |
| `void remove_evens(std::vector<int>& v)` | Remove the even numbers in place, keeping the order of the rest. |

**Fast tier:** `squares` and `decode` must build their result with a **single heap allocation**, and `remove_evens`
must not allocate at all. The tests count allocations, so the tier is about knowing what the vector does for you.
