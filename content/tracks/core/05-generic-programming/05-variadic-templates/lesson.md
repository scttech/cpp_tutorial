# Variadic templates and fold expressions

So far a template had a fixed number of parameters. `std::make_unique<T>(...)`, `std::vector::emplace_back(...)`, `std::tuple`, `std::format` and `std::thread` take **any number** of arguments of **any types**. They are *variadic templates*.

## Parameter packs

```cpp
template <class... Ts>                    // Ts is a "type pack": zero or more types
constexpr std::size_t count_of(const Ts&... args) {   // args is the matching "function parameter pack"
    return sizeof...(Ts);                 // sizeof... counts a pack
}
count_of();              // 0
count_of(1, "a", 2.5);   // 3: Ts = {int, char[2], double}
```

A pack is not a container: you cannot index it or loop over it. You can only **expand** it, which writes the pattern to the left of `...` once per element:

```cpp
g(args...)             // g(a1, a2, a3)
g(h(args)...)          // g(h(a1), h(a2), h(a3))
T(std::forward<Args>(args)...)
```

## Fold expressions (C++17)

A **fold** reduces a pack with a binary operator, no recursion needed:

| Form | Expands to (for a1, a2, a3) |
|---|---|
| `(... + args)` unary left | `((a1 + a2) + a3)` |
| `(args + ...)` unary right | `(a1 + (a2 + a3))` |
| `(0 + ... + args)` binary left | `(((0 + a1) + a2) + a3)` |

A unary fold of an **empty** pack is an error, except for `&&` (gives `true`), `||` (gives `false`) and `,` (gives `void()`). A binary fold has the initial value, so it handles the empty pack: that is why `sum` uses `(0 + ... + args)`.

Folding over the **comma operator** is the idiom for "do this for every argument, in order": `(f(args), ...);`

## Perfect forwarding

```cpp
template <class T, class... Args>
std::unique_ptr<T> make_owned(Args&&... args) {
    return std::unique_ptr<T>(new T(std::forward<Args>(args)...));
}
```

`Args&&` where `Args` is **deduced** is not an rvalue reference: it is a **forwarding reference**. It binds to anything: for an lvalue `Args` is `X&`, for an rvalue it is `X`. Inside the function every parameter has a name, so it is an *lvalue*, whatever it was bound to. `std::forward<Args>(args)` turns it back into what it was: an lvalue stays an lvalue (the constructor copies), an rvalue stays an rvalue (the constructor can move). **Forgetting `std::forward` compiles and works, and silently copies everything.** The tests count the copies.

## Your task

Make the eight stubs in `variadic.hpp` variadic templates:

1. `count_of(args...)`: the number of arguments (constexpr).
2. `sum(args...)`: their sum, `0` for none, the common type for mixed arguments (constexpr).
3. `all_true(args...)`, `any_true(args...)`, `all_equal(first, rest...)` (constexpr).
4. `interpolate(fmt, args...)`: replace each `{}` with the next argument written with `<<`. Extra arguments are ignored; missing ones leave `{}` in the text.
5. `make_owned<T>(args...)`: `new T(...)` with perfect forwarding, in a `std::unique_ptr`.
6. `for_each_arg(f, args...)`: call `f` on every argument, in order, forwarding each.

The tiers: **Works** (the functions), **Robust** (empty and single packs, constexpr, edge cases of `interpolate`, value categories), **Fast** (not a single copy that does not have to be made).
