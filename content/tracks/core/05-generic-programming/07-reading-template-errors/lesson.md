# Debug challenge: Reading Template Errors

Templates have a reputation: *"a single typo produces three pages of errors"*. Some of that is deserved, but most template errors have a simple cause and follow a handful of patterns. Once you can recognise the pattern, the three pages shrink to one line.

This lesson **does not compile**. `generic_bugs.hpp` and `main.cpp` contain six pieces of generic code with one mistake each, and every mistake is reported by the compiler in a *different way*. Your job is to read the errors, not to guess.

## How to read a template error

1. **Read the first error only.** Later errors are often consequences of it. Fix it, recompile, read the next.
2. **Read the notes under it.** For a failed call, Clang adds a note per candidate saying *why* it was ignored: `deduced conflicting types`, `could not match`, `constraints not satisfied ... because ...`.
3. **`in instantiation of ... requested here`** notes are a *call stack of templates*. The error is inside a template, found when the compiler built it for the types you used. The note closest to your own code names the call you need to fix.
4. **An error inside `/usr/include/...`** is reported inside the standard library, but the library is hardly ever wrong: you asked it for something your type does not support. Skip to the note that points at *your* file.

Each error here also has a plain-language explanation next to it in the Problems tab, with the part of the compiler's message it is about.

## The six mistakes

| # | Where | What you will see |
|---|---|---|
| 1 | `largest(3, 4.5)` | a call that matches no function, with a `candidate template ignored` note |
| 2 | `first_of` | a complaint about a *dependent name* |
| 3 | `twice(2.5)` | `constraints not satisfied`, and a chain of `because` notes |
| 4 | `sorted_versions` | an error inside the standard library, with a long instantiation chain |
| 5 | `stack_demo` | the compiler cannot build a type from a class template |
| 6 | `buffer_demo` | an argument that must be a compile-time constant |

They are independent: fix them in any order, but fix the **first error in the list** each time and recompile, as you would on a real project.

The tests also check that your fixes keep the code *correct*, not just compiling: `largest` must return the common type of its two arguments, `twice` must still refuse strings, and so on.

## What to remember

- A template error is reported where the compiler *found* the problem, which can be far from where you *made* it. The `requested here` notes lead you back.
- Most causes are one of: arguments of the wrong type or shape, a missing `typename`, a missing operator or member on your type, a constraint that is not met, a template argument that is not a constant.
- When the message is inside the standard library, ask: *what does this library function require from my type?* (copyable, comparable with `<`, default constructible...)
- C++20 concepts exist to turn the long error into a short one: `because 'std::string' does not satisfy 'Number'`.
