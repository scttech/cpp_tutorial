# Fast path: what's different in C++

- **`std::optional<T>`**: holds a `T` or nothing, stored inline (no heap). `if (opt)`/`has_value()`, `*opt`/`opt->` (undefined if empty), `opt.value()` (throws), `value_or(d)`. `std::nullopt` is the empty value. No `optional<T&>`.
- **`std::variant<A, B, ...>`**: a type-safe union that knows which alternative it holds; its size is the largest alternative plus a tag; default constructs the FIRST alternative. Access with `std::visit`, `std::get_if`, `std::holds_alternative`, `std::get` (throws).
- **`std::visit(visitor, v)`** requires the visitor to accept EVERY alternative: a missing case is a compile error. The `Overloaded` idiom (`struct Overloaded : Ts... { using Ts::operator()...; };` plus a deduction guide) builds a visitor from lambdas.
- **Recursive variants need indirection**: `std::unique_ptr<Expr>` (or `std::vector<Expr>`) in the node types. A `unique_ptr` tree is move-only: write `clone` for a copy.
- **`std::pair`/`std::tuple` + structured bindings**: `auto [a, b] = f();` (C++17). Prefer a named `struct` when the parts have names.
- **`std::span<T>`** (C++20): non-owning (pointer, length). Pass `std::span<const T>` for "any contiguous sequence I only read". `subspan(offset)`, `first(n)`, `last(n)`, `size()`, `empty()`.
- **Views do not own**: a `span` or `string_view` into a container dangles when the container is destroyed or reallocated.
- **`std::expected<T, E>`** (C++23) is the sibling of `optional` that also says WHY there is no value; this course uses `optional` because "no value" is the whole story here.
