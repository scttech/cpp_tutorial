# Test-out: Buffer class

> **Already comfortable with ownership, copying and moving?** Pass this challenge (both the **Works** and **Robust**
> tiers) and Module 4 is marked complete. You can still open any of its lessons later.

## The task

Complete the `Buffer` class in `buffer.hpp`: a block of `int`s on the heap that can be resized, with **value semantics**.
`main.cpp` (read-only) is a small demo: press **Run** to see what it prints.

The class owns its memory through a raw pointer, so it needs all five special members, and each must be right:
the program runs with **AddressSanitizer** and **UBSan**, the allocation tracker counts every `new`, and the tests use
`Buffer` in the places where mistakes show up (a `std::vector` that grows, `std::sort`, assigning to itself).

### The rules

| Member | Behavior |
|---|---|
| `Buffer()` | Empty: no memory at all (a null pointer). |
| `Buffer(n, fill)` | `n` ints, all equal to `fill` (default 0). `explicit`. `n == 0` allocates nothing. |
| copy constructor, copy assignment | **Deep copy**: an independent block with the same values. Safe for `a = a`. Build the copy before releasing the old block. Copying an empty `Buffer` allocates nothing. |
| move constructor, move assignment | Take the block and the size, leave the source **empty** (size 0, no block). **`noexcept`**, and allocate nothing. Safe for `a = std::move(a)`. |
| destructor | Releases the block (`delete[]`): no leak, nothing freed twice. |
| `swap(Buffer&) noexcept` and `swap(a, b)` | Exchange the contents without copying. |
| `size()`, `empty()` | `const`. |
| `operator[]` | Unchecked, like an array (a `const` and a non-`const` version). |
| `at(i)` | Checked: `std::out_of_range` for `i >= size()` (a `const` and a non-`const` version). |
| `begin()`, `end()` | Pointers to the first element and one past the last (`const` and non-`const`): they make the standard algorithms and range-`for` work. |
| `resize(n)` | Keeps the first `min(n, size())` values; new elements are `0`. `n == size()` does nothing. At most one allocation. |
| `operator==` | Equal sizes and equal elements. |

## Tiers

- **Works**: each member does its job, and nothing leaks.
- **Robust**: self-assignment, empty buffers, moved-from buffers, resizing without reading garbage or writing past the end, `noexcept`,
  a vector of buffers that grows, sorts, erases and inserts.
- **Fast**: copies allocate once, and a move, a swap or a sort allocate **nothing**; a vector moves its elements when it grows.
- **Clean**: no `malloc` / `free` (use `new[]` and `delete[]`, or better, think about what in the standard library could hold the memory for you).
