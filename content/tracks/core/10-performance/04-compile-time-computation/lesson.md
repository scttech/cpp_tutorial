# Compile-time computation

> **The fastest code is the code that never runs.** If an answer is known while compiling (a table of constants, a hash of a string literal, a number of bytes), let the compiler compute it. It costs nothing at run time, it can be checked with `static_assert`, and it ends up in the program as a plain constant.

## `constexpr`: "may run at compile time"

```cpp
constexpr int square(int x) { return x * x; }

constexpr int nine = square(3);      // the compiler runs square(3) while compiling
int n = readNumber();
int s = square(n);                   // the same function, called at run time: fine
```

A `constexpr` function can be used **both ways**. It is evaluated at compile time when the context *demands* a constant (initializing a `constexpr` variable, an array size, a template argument, a `case` label, `static_assert`) and otherwise at run time. Since C++14 such a function can have loops, local variables and several statements; since C++20 it can use `std::vector`/`std::string` inside (as long as the memory is released before the constant expression ends), and call `std::sort`.

A `constexpr` **variable** is a constant that is computed at compile time and that the compiler can *see through*:

```cpp
constexpr std::array<int, 4> squares = {square(1), square(2), square(3), square(4)};
static_assert(squares[2] == 9);
```

What is NOT allowed in a constant expression: reading a variable that is not constant, calling a function that is not `constexpr`, `new` that is not freed again, undefined behavior (a signed overflow or an out-of-range index is a **compile error** there, which is a feature).

## `consteval`: "must run at compile time"

```cpp
consteval int checked_size(const char* text) { ... }
int n = checked_size("hello");       // OK, computed by the compiler
int m = checked_size(name);          // ERROR: `name` is not a constant
```

A `consteval` function (an *immediate function*) can **only** be called in a constant expression. Use it when a run-time call would be a mistake, or when the result must be usable where a constant is needed. This lesson's `fnv1a` is a hash of a string: with it you can write

```cpp
switch (hash_of(request)) {
    case fnv1a("GET"):  ...
    case fnv1a("POST"): ...
}
```

and the hashes in the `case` labels are computed by the compiler.

## `constinit`: "initialized at compile time, but not constant"

A global variable with a dynamic initializer (`int x = compute();`) runs code before `main`, in an order between different `.cpp` files that nobody controls: the **static initialization order fiasco**. A `constinit` variable must be initialized by a constant expression, so there is no such code, and the compiler rejects the declaration if the initializer is not constant. Unlike `constexpr`, a `constinit` variable can still be changed at run time:

```cpp
constinit int counter = square(5);   // 25, ready before main; counter++ is fine
```

## Tables at compile time

A function that builds a `std::array` is a lookup table without a run-time cost:

```cpp
constexpr std::array<std::uint8_t, 256> make_table() { std::array<std::uint8_t, 256> t{}; /* fill */ return t; }
inline constexpr auto table = make_table();         // in the program as 256 constant bytes
```

Because the table is `constexpr` it can be used in constant expressions (`static_assert(table[255] == 8)`).

## The compiler has a budget

A constant expression is *interpreted* by the compiler, and the interpreter gives up after a limit of steps (about a million in Clang by default: *"constexpr evaluation hit maximum step limit"*). So algorithms matter at compile time too: a sieve of Eratosthenes for numbers up to 30 000 takes some tens of thousands of steps; trial division, which divides every number by every smaller number, takes millions and is rejected. And remember the other price of compile-time work: **compile time**. A huge table in a header is recomputed in every `.cpp` file that includes it.

## The exercise

`tables.hpp` has five pieces to fill in (the comments give the details): `factorial` (`constexpr`), `fnv1a` (`consteval`), a popcount lookup table built by `make_popcount_table`, `make_sieve<N>`, and a `constinit` counter. The tests *themselves* call these functions in constant expressions, so a function that is not usable at compile time does not compile.

## The tiers

- **Works**: the values, the table entries, `fnv1a` as a `case` label, the first primes, the counter.
- **Robust**: 20! and the overflow guard, non-ASCII text (unsigned bytes), every table entry, 0 and 1 are not prime, a sieve checked against trial division, the end of the array.
- **Fast**: the sieve for 30 000 numbers fits in the compiler's budget, and the table is a real constant.
- **Clean**: `consteval` and `constinit` used where they belong, no macros, no `const_cast`.
