# Fast path: what's different in C++

- No modules by default (C++20 modules exist but are not mainstream yet). Each `.cpp` is compiled independently after textual `#include` expansion (a *translation unit*); the **linker** matches uses to definitions.
- Declaration (`int f(int);`) vs definition (`int f(int x) { ... }`). Declare as often as you like; **define once** program-wide (One Definition Rule). Violations are often *silent* or show up only at link time (`multiple definition`), and some are undefined behavior with no diagnostic at all.
- Headers hold declarations, type definitions, templates, `constexpr` values, and `inline` functions. `.cpp` files hold non-inline definitions.
- Every header needs an include guard (`#pragma once` or `#ifndef` triple). Otherwise double inclusion causes "redefinition" errors.
- `inline` on a function means "multiple identical definitions across translation units are allowed", not "please inline this".
- Namespace-scope `const`/`constexpr` variables have internal linkage, so they are safe in headers; non-const globals are not (use `inline` variables in C++17+).
