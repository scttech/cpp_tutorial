# Arrays and `std::array`

A fixed-size sequence of values stored side by side in memory. C++ has two kinds, and you should know both.

## C arrays

```cpp
int raw[4] = {10, 20, 30, 40};   // 4 ints, 16 bytes, size fixed at compile time
raw[2];                          // 30
sizeof(raw);                     // 16 : the whole array
sizeof(raw) / sizeof(raw[0]);    // 4  : the element count
```

They are inherited from C and have sharp edges:

- **No bounds checking.** `raw[4]` and `raw[-1]` compile and run. They read whatever happens to be next to the array:
  that is *undefined behavior*. It might return garbage, might crash, might appear to work today.
- **Decay.** When you pass an array to a function, it silently turns into a pointer to its first element:

```cpp
void f(const int* p) {
    sizeof(p);   // 8 : the size of a *pointer* (on a 64-bit machine), not of the array
}
f(raw);          // raw "decays" to &raw[0]; the length is lost
```

  That is why C-style functions take a pointer **and a count**. A parameter written `int p[4]` or `int p[]` is *still
  just a pointer*: the `4` is ignored.
- They cannot be copied with `=`, returned from a function, or compared with `==` (that compares addresses).

## `std::array`

```cpp
#include <array>
std::array<int, 4> a{10, 20, 30, 40};
a.size();     // 4, remembered by the type
a[2];         // 30 : still unchecked, like a C array
a.at(2);      // 30 : checked: throws std::out_of_range if the index is bad
auto b = a;   // a real copy
a == b;       // true: compares the elements
```

`std::array<T, N>` is a thin wrapper around a C array with the same size and speed, but it behaves like a normal
value: it can be copied, returned, compared, and it never decays (it knows its own `size()`). Prefer it whenever the
size is known at compile time. Nest them for a grid: `std::array<std::array<int, 3>, 2>` is 2 rows of 3.

(`at()` needs exceptions, which you will meet later. In this lesson you will check the index yourself.)

## Indexes are unsigned

`size()` and the right type for an index are `std::size_t`, which is **unsigned**. Converting `-1` to `std::size_t`
gives a huge number, not a negative index, so one comparison `index >= size` rejects both "too big" and "negative".

## Your task

`matrix.hpp` (read-only) defines `Matrix2x3` and `Matrix3x2`. Implement these in `main.cpp`:

| Function | Behavior |
|---|---|
| `Matrix3x2 transpose(const Matrix2x3& m)` | Swap rows and columns: `result[c][r] == m[r][c]`. |
| `long long sum_range(const int* data, std::size_t count)` | Sum of the first `count` elements. `0` for a count of 0 (the pointer may then be null). Must not overflow when the values are large. |
| `int at_or(const std::array<int, 6>& a, std::size_t index, int fallback)` | `a[index]` if the index is valid, else `fallback`. Never reads outside the array. |
| `std::size_t index_of(const std::array<int, 6>& a, int value)` | Index of the first element equal to `value`, or `a.size()` if there is none. |

The `main` in the starter prints `sizeof` for an array and for the pointer a function receives: run it before you
start, and check that the two numbers match what you just read.
