# Fast path: what's different in C++

How to read a template error (Clang; GCC is similar but wordier):

1. **Read the FIRST error, only.** The rest are often consequences. Fix it, recompile.
2. **The last line of the first message block is usually the cause**: Clang prints notes that explain it (`candidate template ignored: ...`, `because ...`).
3. **`in instantiation of ... requested here`** notes are the *call stack of templates*: the error is inside a template, found while building it for the types YOU used. The LAST such note that is in your file is the call to fix.
4. **An error inside `/usr/include/...`** (the standard library) is almost never a bug in the library: you asked it for something your type does not support (no `operator<` for `std::sort`, not copyable in a `std::vector` copy...). Find the note that points at your file.
5. Know the common message shapes:
   - `deduced conflicting types for parameter 'T' ('int' vs. 'double')`: one `T`, two different argument types.
   - `couldn't infer template argument 'T'`: `T` appears in no function parameter; name it: `f<int>()`.
   - `could not match 'vector<T>' against 'int'`: argument has the wrong *shape*.
   - `constraints not satisfied` + `because 'X' does not satisfy 'Concept'`: a concept requirement failed.
   - `missing 'typename' prior to dependent type name`: write `typename T::type`.
   - `no viable constructor or deduction guide`: write the template arguments, `Stack<int>`.
   - `non-type template argument is not a constant expression`: use `constexpr`.
   - `static assertion failed due to requirement ...`: a `static_assert` in the template says what is wrong, in the author's words.
