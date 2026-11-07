# Fast path: what's different in C++

- Ahead-of-time compiled: preprocess -> compile each `.cpp` -> link. No runtime or VM to start.
- `#include` is textual copy-paste, not a module import (C++20 modules exist but are not used here).
- `int main()` is the entry point; returning `0` (or nothing) means success.
- Output: `std::cout << value << '\n';`. Prefer `'\n'` over `std::endl` (which also flushes).
- Errors you will meet first: missing `#include`, missing `std::`, missing semicolon. Fix the first error first.
