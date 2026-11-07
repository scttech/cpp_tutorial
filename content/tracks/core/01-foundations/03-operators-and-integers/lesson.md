# Operators and integer behavior

The arithmetic operators are the familiar `+ - * / %`. What is different in C++ is what happens at the edges.

## Division and remainder

```cpp
7 / 2     //  3   integer division truncates toward zero
-7 / 2    // -3   (not -4)
7 % 2     //  1
-7 % 5    // -2   the remainder takes the sign of the dividend, so it can be negative!
```

`a % m` is *not* a mathematical "mod" for negative `a`. If you want a result in `[0, m)`, you have to fix it up.
Dividing by zero (integers) is undefined behavior.

## Signed overflow is undefined behavior

```cpp
int x = std::numeric_limits<int>::max();
int y = x + 1;     // UNDEFINED BEHAVIOR. Not "wraps around". The compiler may assume it never happens.
```

Because the compiler is allowed to assume signed overflow never occurs, it can optimize in surprising ways (for
example, deleting an `if (x + 1 < x)` check). **You must check before you add**, not after.

**Unsigned overflow is different:** it is well-defined and wraps around modulo 2^N.

```cpp
unsigned int u = 0;
u = u - 1;         // 4294967295 on a 32-bit unsigned: defined, but often a bug
```

## Bitwise operators

```cpp
x & y    // and        x | y    // or        x ^ y    // xor
~x       // not        x << n   // shift left x >> n    // shift right
```

Use unsigned types for bit manipulation; shifting signed values has several sharp edges. Fixed-width types live in
`<cstdint>`: `std::uint32_t`, `std::int64_t`, and so on.

A classic trick: `x & (x - 1)` clears the lowest set bit of `x`.

## Implicit conversions

Small integer types are promoted to `int` in arithmetic, and mixing signed and unsigned converts the signed value
to unsigned. `-1 < 1u` is **false**, because `-1` becomes `4294967295u`. Your compiler warns about many of these
with `-Wall -Wextra`; read the warnings.

## Your task

Implement these in `main.cpp` (keep the names and signatures):

| Function | Behavior |
|---|---|
| `bool safe_add(int a, int b, int& out)` | If `a + b` fits in an `int`, store it in `out` and return `true`. Otherwise return `false` **and leave `out` unchanged**. You must not trigger overflow while checking. |
| `int floor_mod(int a, int m)` | For `m > 0`, return the remainder in `[0, m)`, even when `a` is negative. `floor_mod(-1, 5)` is `4`. |
| `bool is_power_of_two(unsigned int x)` | True for 1, 2, 4, 8, ...; false for 0. Use a bitwise trick, not a loop. |
| `int count_set_bits(std::uint32_t x)` | The number of 1 bits in `x`. |

The **Works** tests use ordinary values. The **Robust** tests probe the edges: `INT_MIN`, `INT_MAX`, zero, and the
all-ones pattern.
