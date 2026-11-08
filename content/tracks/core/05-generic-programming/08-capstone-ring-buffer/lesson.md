# Capstone 1: RingBuffer&lt;T, N&gt;

A **ring buffer** (circular buffer) is a queue with a fixed capacity whose storage is reused forever: audio samples, the last N log lines, a producer/consumer hand-off, a network card's receive queue. There is no allocation after construction, and push and pop are O(1).

This capstone uses the whole module: a **class template** with a **non-type parameter** (`N`), **concepts** that constrain it, a **variadic, perfectly forwarding** `emplace`, and an **iterator class template** that serves for both `iterator` and `const_iterator`.

## The idea

```
capacity N = 5        slot:  0   1   2   3   4
after push 10,20,30,40       10  20  30  40  --        head_ = 0, size_ = 4
after pop, pop                -   -  30  40  --        head_ = 2, size_ = 2   (10 and 20 left)
after push 50,60,70           70  -  30  40  50 60?    ... the next free slot wraps back to 0
```

Two numbers are enough: `head_` (the array index of the OLDEST element) and `size_`. The element at **logical position** `i` (0 = the oldest) is in array slot `(head_ + i) % N`; the next free slot is `(head_ + size_) % N`. The `%` is what makes the buffer a ring.

```cpp
RingBuffer<int, 4> b;
b.push(1);                     // true
b.push(2); b.push(3); b.push(4);
b.push(5);                     // false: full, nothing changes
auto oldest = b.pop();         // std::optional<int>: 1
b.push_overwrite(6);           // when full the OLDEST is replaced instead of refusing
for (int x : b) { ... }        // oldest to newest
```

## The interface

| Member | What it does |
|---|---|
| `capacity()`, `size()`, `empty()`, `full()` | |
| `push(const T&)`, `push(T&&)` | add at the back; `false` (and no change) when full |
| `emplace(args...)` | like `push`, building the element from the arguments (perfect forwarding) |
| `push_overwrite(T)` | add at the back; when full, replace the oldest |
| `pop()` | remove the oldest: `std::optional<T>` (empty when the buffer is empty) |
| `clear()` | remove everything *and let go of what the elements held* |
| `front()`, `back()`, `operator[]` | access by logical position (oldest = 0), no bounds checking |
| `begin()`, `end()` (+ `const`, `cbegin`, `cend`) | forward iterators, oldest first |

## Constraining the template

```cpp
template <std::default_initializable T, std::size_t N>
    requires(N > 0) && std::movable<T>
class RingBuffer
```

The storage is a `std::array<T, N>` full of default-constructed elements, so `T` must be default constructible, and elements are moved in and out, so `T` must be movable. A capacity of 0 makes no sense. With the constraints, a mistake gives `constraints not satisfied ... because 'NoDefault' does not satisfy 'default_initializable'` at the place where the type is named, instead of an error deep inside the class.

## The iterator

One class template serves for both iterators:

```cpp
template <bool Const> class Iter {   // Const = true for const_iterator
    using Owner = std::conditional_t<Const, const RingBuffer, RingBuffer>;
    ...
    reference operator*() const { return (*owner_)[index_]; }
```

It stores a pointer to the buffer and a **logical position**, so wrapping is already handled by `operator[]`. An `iterator` converts to a `const_iterator` but not the other way round (`operator Iter<true>() const requires(!Const)`). It needs the five typedefs (`iterator_category`, `value_type`, `difference_type`, `pointer`, `reference`) so that standard algorithms accept it.

## Tiers

- **Works**: every operation, wrapping around, the iterators, the standard algorithms, the constraints.
- **Robust**: capacity 1, long random sequences checked against a `std::deque`, `shared_ptr` elements (does `clear` really release them? does `pop` leave a second owner?), move-only elements, copies that are independent, iterator conversions.
- **Fast**: **no heap allocation at all**, and **no copy that can be a move**: `push(T&&)` moves, `push(const T&)` copies exactly once, `pop()` moves exactly once, `emplace` forwards.
- **Clean**: no `new`/`delete`/`malloc`, no dynamic container: the storage is the `std::array` member.

## Hints for the order of work

1. Do `capacity`/`size`/`empty`/`full`, `slot()`, `push`, `operator[]`, `pop`: the first tests pass.
2. `front`/`back`, `push_overwrite`, `clear`, `emplace`.
3. The iterators last. Write the typedefs first: most "no matching function" errors in algorithms come from a missing one.
