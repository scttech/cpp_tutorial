# Fast path: what's different in C++

- A **parameter pack** is a list: `template <class... Ts> void f(Ts... args)`. `sizeof...(Ts)` is its length. You cannot index it; you **expand** it (`g(args...)` becomes `g(a1, a2, a3)`; the pattern left of `...` is repeated, so `g(h(args)...)` is `g(h(a1), h(a2), h(a3))`) or **fold** it.
- A **fold expression** reduces a pack with an operator: `(... + args)` unary left, `(args + ...)` unary right, `(0 + ... + args)` binary left (has an initial value). An empty pack is an error for a unary fold, except `&&` (true), `||` (false) and `,` (void()).
- Folding over `,` is the idiom for "do this for every argument, in order": `(f(args), ...);`
- `T&&` with `T` *deduced* is a **forwarding reference**: it binds to lvalues and rvalues. `std::forward<T>(x)` re-applies the original value category: lvalue stays lvalue (copy), rvalue stays rvalue (move). Forgetting it silently copies everything (an unnamed rvalue becomes a named lvalue).
- `Args&&... args` + `std::forward<Args>(args)...` is **perfect forwarding**: what `std::make_unique`, `emplace_back` and `std::thread` do.
- No `printf` format-string bugs: variadic templates are type-safe, because every argument keeps its real type.
- Function templates can be `constexpr`: the folds above run at compile time when the arguments are constants.
