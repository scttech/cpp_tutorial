# Fast path: what's different in C++

- **A tree of `std::unique_ptr`.** Ownership is the shape: each node owns its children, destroying the root destroys the tree, nothing leaks, nothing is shared. In Java or Python the garbage collector does this; in C++ it is a type. Moving a `unique_ptr` is free, so rewriting a tree means *moving pointers*, not copying nodes.
- **`std::variant` + `std::get_if` instead of virtual functions.** The node kinds are a closed set, so a variant stores them inline and the compiler can check you handled each. (Module 7's class hierarchy is the open-set alternative: new node kinds without touching old code.)
- **Errors as return values.** A calculator sees bad input constantly; `UnexpectedEnd` at offset 5 is data, not an exception. C++ compilers (Clang, GCC) report many errors in one run for the same reason.
- **Recursive descent has a cost: the stack.** Hostile input like `((((…` recurses once per character. Compilers put a limit on nesting for exactly this reason (Clang: 256 template or bracket levels by default).
- **`std::string_view`, `std::from_chars`, `strtod`:** the lexer reads the original text in place; only identifiers need an owned `std::string`.
- **Optimizers must preserve behaviour, floating point included.** Real compilers keep `x*0` unless you pass `-ffast-math`, for the same reasons as here: NaN, infinity and errors.
