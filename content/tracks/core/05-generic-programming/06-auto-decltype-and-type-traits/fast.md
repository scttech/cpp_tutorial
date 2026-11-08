# Fast path: what's different in C++

- **`auto` deduces like a template parameter by value**: it drops references and top-level `const` (`auto x = ref;` is a COPY). `auto&`, `const auto&` and `auto&&` keep them.
- **`decltype(expr)`** is the exact type of an expression (or of a name, if it is a bare name), never evaluated. **`decltype(auto)`** means "deduce with decltype rules": `decltype(auto) f() { return *p; }` returns a reference, `return (x);` (parenthesized!) returns `T&`, `return x;` returns `T`.
- **`std::declval<T>()`** is a pretend `T` value, for use in unevaluated contexts (`decltype`, `noexcept`, `requires`), when `T` may not be default constructible.
- A **type trait** is a class template computing a type (`::type`, with an alias `_t`) or a value (`::value`, with a `_v` variable). Write one with partial specialization; inherit from `std::true_type` / `std::false_type` for yes/no answers.
- **Detection idiom**: a primary template that says no, and a partial specialization `<T, std::void_t<expr...>>` that says yes whenever every `expr` is valid (an invalid one is SFINAE: the specialization silently drops out).
- Traits + **`if constexpr`** give one function with different code per type (`std::is_trivially_copyable_v<T>` -> `memcpy`). In a template the discarded branch is not instantiated.
- `<type_traits>` has the standard ones: `std::remove_cvref_t`, `std::decay_t`, `std::is_same_v`, `std::invoke_result_t`, `std::is_nothrow_invocable_v`, `std::is_array_v`, `std::extent_v`... Re-implementing a few is how you learn to read their errors.
