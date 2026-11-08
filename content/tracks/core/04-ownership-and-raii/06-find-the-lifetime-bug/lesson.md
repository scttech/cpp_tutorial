# Debug challenge: Lifetime Bugs

Six small functions. Each has **one** bug, and every bug is the same *kind* of mistake: **something is used after the thing it
refers to is gone**. This is the most common source of crashes, security holes and "works on my machine" in C++, and it is
what everything in this module (RAII, ownership, smart pointers) exists to prevent.

All six compile, most even look right, and several *seem* to work, until the memory they point at is reused.

The signatures are in `bugs.hpp` and the bodies in `main.cpp`. A bug can be in either file, and a fix may change a return type (change it
in both files together): the tests only use the results.

## Your tool: the sanitizer

This lesson runs with **AddressSanitizer** and **UndefinedBehaviorSanitizer** switched on. When the program touches memory it should not,
the sanitizer stops it and tells you:

- **what** happened (`heap-use-after-free`, `stack-use-after-return`, `double-free`, ...),
- **where** (file and line of the bad access),
- and, for memory errors, **where the memory was allocated and freed**.

**Press Run.** The program stops at the first bug, and the panel explains it in plain language and points at the line. Fix it, run again, and
the next one shows itself. In the tests, a sanitizer report ends only the test it happened in (it shows as *Crashed*), so you can see
which functions are still wrong at a glance.

## The six bugs

| # | Function | What goes wrong | Family |
|---|---|---|---|
| 1 | `longest_word` | returns a reference to a local variable | dangling reference |
| 2 | `label_of` | returns a `string_view` of a temporary string | dangling view |
| 3 | `remove_evens` | erases from a vector while looping over it | iterator invalidation |
| 4 | `make_counter` | the returned lambda captures a local by reference | dangling capture |
| 5 | `use_twice` | two `shared_ptr`s built from the same raw pointer | double free |
| 6 | `describe` | returns `c_str()` of a temporary `std::string` | dangling pointer |

## What to remember

- A reference, pointer, `string_view`, iterator or lambda capture **refers** to something. It does not keep it alive. Always ask:
  *what does it refer to, and is that still alive here?*
- **Return by value** (a `std::string`, a `std::vector`). Moves and copy elision make it cheap, and the result is the caller's own.
- A `string_view` or `c_str()` is only valid as long as the string it came from is. A temporary lives until the end of the full expression.
- `erase` and `push_back` can invalidate iterators. Use `erase(remove_if(...), end())`, or `it = v.erase(it)` and increment only when you did not erase.
- A lambda that outlives its function must **capture by value** (`[n = start]() mutable { ... }`).
- **Never create two `shared_ptr`s from one raw pointer**: each gets its own reference count. Use `make_shared` and *copy* the `shared_ptr`.
- Static buffers are not a fix: a second call overwrites the first result. Ownership is.

The *Works* tier has one test per bug. The *Robust* tier checks that your fix is a real one: results that stay valid after other calls, several results alive at once,
no leaks, and edge cases (consecutive evens, empty input). The *Clean* tier wants `main.cpp` free of `new` and `delete`.
