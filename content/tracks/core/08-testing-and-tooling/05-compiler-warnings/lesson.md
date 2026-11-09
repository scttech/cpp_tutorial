# Compiler warnings: your first static analyzer

The cheapest bug-finding tool you have is already installed: **the compiler itself**. It reads your whole program and knows the types of every expression; with the right switches it will tell you about code that is legal but almost certainly wrong. Every warning is a free, instant code review. Experienced C++ programmers turn warnings up as far as they can stand and treat each one as a bug until proven otherwise.

## Which warnings to turn on

`-Wall` and `-Wextra` are the minimum (the course has used them from the start). A strict project adds:

| flag | catches |
|---|---|
| `-Wall -Wextra` | unused variables and parameters, missing `return`, uninitialized variables, comparisons of signed and unsigned, `switch` that forgets an enumerator, assignment used as a condition, self-assignment, and many more |
| `-Wpedantic` | things that are not standard C++ but that your compiler accepts as an extension: variable-length arrays, `;` after a function body, `0b` literals in old standards |
| `-Wconversion` | **implicit conversions that can lose information**: `double` to `int`, `int` to `char`, `long` to `int`, `size_t` to `int` |
| `-Wshadow` | a variable that **hides** another one of the same name in an enclosing scope |
| `-Werror` | turns every warning into an error, so the build fails and nobody can ignore it |

More worth knowing (Clang and GCC both): `-Wsign-conversion`, `-Wold-style-cast`, `-Wnon-virtual-dtor`, `-Woverloaded-virtual`, `-Wnull-dereference`, `-Wdouble-promotion`, `-Wformat=2`, `-Wimplicit-fallthrough`; GCC adds `-Wuseless-cast`, `-Wduplicated-cond`, `-Wlogical-op`. Clang's `-Weverything` turns on every warning there is: use it once to see what exists, not as a policy.

## `-Werror`: when, and when not

- **In CI and for your own code: yes.** A warning that is allowed to stay turns into a hundred warnings that nobody reads.
- **For dependencies: no.** Include third-party headers as *system* headers (`-isystem`, or the `SYSTEM` keyword in CMake) and the compiler stays quiet about code you do not own. This is why a mistake *inside* GoogleTest does not warn.
- **A new compiler version adds warnings.** Teams that ship code compiled by many compilers often use `-Werror` only on the compiler they build releases with, or `-Werror=<specific-warning>` for the ones that are always bugs.

## Never silence a warning; remove its cause

There are three ways to make a warning disappear, and only one of them is good:

1. **Fix the code** so that the warning has no reason to exist. Always the first choice.
2. **Say what you mean**: when the conversion really is intended, write it out: `static_cast<int>(x)`. A cast documents *"I know this loses information"*. But first ask whether it loses information that you need, as this lesson's examples show: a cast to `int` *truncates*, and you may have wanted to *round*.
3. **Suppress** it, locally and with a comment saying why (`#pragma clang diagnostic ignored`, `[[maybe_unused]]`, `(void)param`). Only for a warning that is really a false alarm, never to get past a bug.

## Your task: nine warnings, nine bugs

`metrics.hpp` holds nine small pieces of code. The lesson builds with

```
-Wall -Wextra -Wpedantic -Wconversion -Wshadow -Werror
```

so **the starter does not compile**: press **Run Tests** and the Problems tab lists the warnings as errors. Each one points at a real bug (the comment above each piece says what it should do). Read the message, understand the bug, fix the **code**. Press **Run** to see the program in `main.cpp` once it builds.

Some hints about what to look for in the messages: *explicitly assigning value of variable to itself*; *declaration shadows a local variable*; *implicit conversion turns floating-point number into integer*; *implicit conversion loses integer precision*; *unused parameter*; *variable is used uninitialized whenever ...*; *enumeration value not handled in switch*; *using the result of an assignment as a condition without parentheses*; *variable length arrays in C++ are a Clang extension*.

- **Works**: each function does what its comment says.
- **Robust**: the edge cases that the careless fixes get wrong: rounding of negative numbers, clamping instead of wrapping, boundaries, every enumerator, millions of terms.
- **Clean**: no `#pragma` / `-Wno` / `[[maybe_unused]]` / `(void)` / C-style cast hiding a problem, no run-time sized array.
