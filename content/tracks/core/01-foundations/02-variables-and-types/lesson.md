# Variables and fundamental types

C++ is **statically typed**: every variable has a type fixed at compile time.

```cpp
int count = 3;          // whole number, typically 32 bits
double ratio = 0.75;    // 64-bit floating point
char letter = 'A';      // one byte; holds a character code
bool ready = true;      // true or false
```

## Initialization: prefer braces

```cpp
int a = 7;     // copy-initialization
int b(7);      // direct-initialization
int c{7};      // brace (list) initialization, preferred
int d{};       // zero-initialized: 0
```

Braces **refuse narrowing conversions** that might lose information:

```cpp
int x{3.9};    // compile error: double -> int narrows
int y = 3.9;   // compiles silently; y is 3
```

## `auto`, `const`, `constexpr`

```cpp
auto n = 42;            // deduced as int
auto pi = 3.14159;      // deduced as double
const int kLimit = 100; // cannot be modified after initialization
constexpr double kMilesPerKm = 0.621371;  // value known at compile time
```

Use `const` by default for anything that should not change. `constexpr` values can be used where the compiler
needs a constant, for example array sizes.

## The traps

**Uninitialized variables are undefined behavior.**

```cpp
int total;              // indeterminate value: NOT zero
std::cout << total;     // undefined behavior: may print anything, or "work" today and break tomorrow
```

Always initialize: `int total{};` or `int total = 0;`.

**Integer division truncates.** If both operands are integers, the result is an integer:

```cpp
double half = 1 / 2;      // 0, because 1 / 2 is integer division, then converted to 0.0
double half2 = 1 / 2.0;   // 0.5
double ratio = 5.0 / 9.0; // 0.5555...  (5 / 9 would be 0!)
```

**Sizes are not fixed.** `int` is *at least* 16 bits and usually 32. When you need an exact size, use
`<cstdint>` types such as `std::int32_t`. You will meet those in the next lesson.

## Your task

Implement the five unit-conversion helpers in `main.cpp`. The tests call them directly, so keep the names and
signatures exactly as given.

| Function | Behavior |
|---|---|
| `double celsius_to_fahrenheit(double)` | `c * 9/5 + 32` |
| `double fahrenheit_to_celsius(double)` | `(f - 32) * 5/9` |
| `double km_to_miles(double)` | multiply by `0.621371` (use a `constexpr` constant) |
| `int minutes_to_seconds(int)` | whole minutes to seconds |
| `double average_of_two(int, int)` | the exact average, e.g. `average_of_two(1, 2)` is `1.5` |

Watch the integer-division trap in at least two of them.
