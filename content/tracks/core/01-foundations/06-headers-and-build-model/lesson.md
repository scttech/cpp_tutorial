# Headers, source files, and the build model

Real programs span many files. To understand how they fit together you need the build model from lesson 1 in more
detail.

## Translation units

The compiler never sees your whole program. It compiles each `.cpp` file separately, after the preprocessor has
pasted in every `#include`d header. The result of that (one `.cpp` plus everything it includes) is a
**translation unit**. Each translation unit becomes an object file. The **linker** then stitches object files
together, matching each *use* of a function to exactly one *definition*.

```
main.cpp   --preprocess+compile-->  main.o   \
circle.cpp --preprocess+compile-->  circle.o  >--link-->  program
```

## Declarations vs definitions

```cpp
double area(const Circle& c);                          // declaration: "this function exists"
double area(const Circle& c) { return /* ... */; }     // definition: the actual body
```

A declaration tells the compiler enough to type-check a call. The definition is what the linker needs. A name can
be **declared** many times but **defined** only once in the whole program. That is the heart of the
**One Definition Rule (ODR)**.

## What goes where

- **Header (`.hpp`)**: declarations that other files need: `struct`/`class` definitions, function declarations,
  `constexpr` constants, templates, and `inline` functions.
- **Source (`.cpp`)**: definitions of the functions declared in the header.

Because a header is pasted into *every* file that includes it, anything in a header must be safe to appear in many
translation units.

## Include guards

If one file includes `a.hpp` and also `b.hpp`, which itself includes `a.hpp`, then the contents of `a.hpp` appear
twice in that translation unit, and a `struct` defined twice is an error. An **include guard** prevents that:

```cpp
#ifndef MYPROJECT_CIRCLE_HPP
#define MYPROJECT_CIRCLE_HPP
// ... contents ...
#endif
```

`#pragma once` does the same job and is supported everywhere that matters. Use one or the other in **every** header.

## The classic mistake: definitions in headers

```cpp
// circle.hpp
double area(const Circle& c) { return 3.14 * c.radius * c.radius; }   // BAD
```

Every `.cpp` that includes this header now has its *own definition* of `area`. The compiler is happy (each translation
unit alone is fine) but the linker finds several definitions and reports `multiple definition of 'area'`. Fixes:
move the body into a `.cpp`, or mark the function `inline` (which tells the linker that duplicate identical
definitions are fine, as `shapes.hpp` does for `total_area`).

## Your task

The program is split into four files. `main.cpp` and `shapes.hpp` are provided (read-only). You write the other two:

- **`circle.hpp`**: with an include guard, a `struct Circle { double radius; };` and *declarations* of
  - `double area(const Circle&)`
  - `double circumference(const Circle&)`
  - `bool contains(const Circle& c, double dx, double dy)`: is the point at offset `(dx, dy)` from the circle's center inside or on the circle?
- **`circle.cpp`**: the definitions. Use `std::numbers::pi` from `<numbers>` for pi.

Notice that `main.cpp` includes both `circle.hpp` and `shapes.hpp`, and `shapes.hpp` includes `circle.hpp` again. Until
you add the include guard, the build will fail with a "redefinition of 'Circle'" error. That is the point.
