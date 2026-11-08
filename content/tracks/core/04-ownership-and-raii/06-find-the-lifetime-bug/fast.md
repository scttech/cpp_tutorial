# Fast path: what's different in C++

Every bug here is **use after lifetime end**. The fixes, in one line each:

- Returning a reference/pointer/view to a **local or temporary**: return the **value** (`std::string`, `std::vector`), not `const T&`, `string_view` or `c_str()`. A temporary lives until the end of the full expression.
- **Erasing while iterating** invalidates the iterator: erase-remove idiom (`v.erase(std::remove_if(...), v.end())`) or `it = v.erase(it)` (increment only when nothing was erased).
- A **lambda that escapes** its function captures **by value** (`[n = start]() mutable { return n++; }`), never `[&]` for a local.
- **Never two `shared_ptr`s from the same raw pointer** (two reference counts, double free). `std::make_shared`, then copy the `shared_ptr`.
- A static buffer is not ownership: the next call overwrites the previous result.
- Use `-fsanitize=address,undefined` while developing: use-after-free, use-after-return, double free, leaks and overflows become exact reports with the line and the allocation/free stacks.
