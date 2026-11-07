# Hello, World (and what just happened)

Every C++ program starts running at a function named `main`. Here is the smallest useful program:

```cpp
#include <iostream>

int main() {
    std::cout << "Hello, World!\n";
    return 0;
}
```

## What each part does

- `#include <iostream>` is a **preprocessor** directive. Before compiling, the preprocessor pastes the contents of
  the `iostream` header into your file. That header *declares* `std::cout`, so the compiler knows it exists.
- `int main()` is the entry point. It returns an `int` to the operating system: `0` means success.
- `std::cout` is the standard output stream. `<<` sends things to it. The `std::` prefix is the **namespace**
  the standard library lives in.
- `"\n"` is a newline. (`std::endl` also writes a newline but additionally *flushes* the stream, which is slower.)

## From source to running program

C++ is compiled ahead of time, in stages:

1. **Preprocess**: handle `#include`, `#define`, and friends.
2. **Compile**: turn each `.cpp` file (a *translation unit*) into machine code in an object file.
3. **Link**: combine object files and libraries into one executable.

When you press **Run**, this site does all three steps for you, then runs the result.

## Your task

Edit `main.cpp` so the program prints exactly `Hello, World!` followed by a newline. Then press **Run Tests**.

## Things that surprise people coming from other languages

- You may omit `return 0;` in `main` (the compiler adds it), but *only* in `main`. Any other function that
  promises to return a value and does not has undefined behavior.
- A missing `#include` is a compile error, not a runtime one. Read the *first* error the compiler reports;
  later ones are often fallout.
