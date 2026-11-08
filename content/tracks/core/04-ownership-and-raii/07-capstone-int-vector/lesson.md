# Capstone: Your own vector

`std::vector` is the container you have used since Module 2. In this capstone you write the heart of it yourself: a growable array of
`int`s that owns a block of heap memory. Everything from Module 4 is here: **RAII**, the **rule of five**, **move semantics**,
aliasing hazards, and the habit of reasoning about **who owns what, and for how long**. The tests run under the sanitizers and count
every allocation.

`main.cpp` (read-only) is a demo. The names and signatures in `int_vector.hpp` are fixed (the tests call exactly these); every stub
compiles but does nothing useful yet.

## The representation

```
 data_ ──►[ 3 | 1 | 4 | 1 | 5 | ? | ? | ? ]      size_ = 5 elements in use
          └─────── capacity_ = 8 ──────────┘      room for 8 before the block must be replaced
```

The invariant: `size_ <= capacity_`, and `data_` is null exactly when `capacity_` is 0. An empty vector owns **no block**, so moving
and copying an empty one never allocates.

## Why capacity? Amortized growth

If `push_back` allocated a block one element bigger every time, `n` push_backs would copy `1 + 2 + ... + n` elements: quadratic.
Doubling the capacity when it is full makes the total work linear: each element is copied about twice on average. That is why the
tests count allocations: 1000 push_backs must cost about 11 allocations (capacities 1, 2, 4, ..., 1024), not 1000.

## The members

| Member | Behavior |
|---|---|
| `IntVector()` | Empty, capacity 0, no block. |
| `explicit IntVector(count, value = 0)` | `count` copies of `value`; capacity == `count`. |
| `IntVector(std::initializer_list<int>)` | The listed values; capacity == size. **`IntVector(3, 7)` is three 7s, but `IntVector{3, 7}` is the two values 3 and 7**: braces prefer the initializer-list constructor. |
| copy constructor, copy assignment | Deep copy; the copy's capacity == its size. Safe for `a = a`. |
| move constructor, move assignment | Take the block, size and capacity; leave the source with size 0, capacity 0 and no block. `noexcept`. Safe for `a = std::move(a)`. |
| destructor | Releases the block with `delete[]`. |
| `swap(IntVector&) noexcept` + `swap(a, b)` | Exchange everything. |
| `size()`, `capacity()`, `empty()` | `const`. |
| `operator[]` | Unchecked (const and non-const). |
| `at(i)`, `front()`, `back()` | Checked: `std::out_of_range` for a bad index, and for `front`/`back` of an empty vector. |
| `data()`, `begin()`, `end()` | Pointers (const and non-const). |
| `push_back(const int&)` | Adds at the end. When full, the capacity **doubles** (0, then 1, 2, 4, 8, ...). |
| `pop_back()` | Removes the last element (`std::out_of_range` if empty). Capacity unchanged. |
| `insert(pos, const int&)` | Inserts before `pos` (`0..size()`; `std::out_of_range` beyond). Later elements move up. Grows like `push_back` when full. |
| `erase(pos)` | Removes the element at `pos` (`< size()`; `std::out_of_range` otherwise). Later elements move down. Capacity unchanged. |
| `clear()` | Size 0; the capacity and the block **stay**. |
| `reserve(n)` | If `n > capacity()`, the capacity becomes **exactly** `n`. Never shrinks. Size unchanged. |
| `shrink_to_fit()` | Capacity becomes `size()` (no block if empty). |
| `resize(n, const int& value = 0)` | Size becomes `n`; new elements are copies of `value`; capacity is at least `n`. |
| `operator==` | Equal sizes and equal elements. |

## The traps

- **Aliasing again.** `push_back`, `insert` and `resize` take `const int&`, and the argument can be an element of the vector itself:
  `v.push_back(v[0])`. If the vector reallocates, `v[0]` lives in the block you are about to free: read the value first. `insert`
  has a second version of the trap even without reallocation: shifting the elements up can move the very element `value` refers to.
- **A clean separation of "allocate", "copy", "release".** Build the new block, copy into it, and only then release the old one, so that
  a failure in the middle leaves the vector as it was.
- **`new[]` goes with `delete[]`**, and `new int[0]` is an allocation too: use a null pointer for "no block".
- **No standard container inside**, and no `malloc`: the **Clean** tier checks `int_vector.hpp`.

## Tiers

- **Works**: every member does its job, and nothing leaks.
- **Robust**: errors, self-assignment, the aliasing cases, empty vectors, moved-from vectors, a `std::vector` of `IntVector`s, `noexcept`,
  a long random sequence of operations compared with `std::vector`, and a hundred thousand `push_back`s.
- **Fast**: the allocation counts above: geometric growth, `reserve` first means no more allocations, moves and swaps allocate nothing,
  inserting within the capacity allocates nothing.
- **Clean**: no `malloc`/`free`, no standard container.
