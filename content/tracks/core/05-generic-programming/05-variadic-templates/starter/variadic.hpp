#pragma once

#include <cstddef>
#include <memory>
#include <sstream>
#include <string>
#include <string_view>
#include <utility>

// A variadic template takes ANY number of arguments of ANY types. `class... Ts` is a "parameter pack": a list of types;
// `Ts... args` is a pack of the matching arguments. You cannot index a pack: you EXPAND it (`args...` becomes `a1, a2, a3`) or
// FOLD it (`(args + ...)` becomes `a1 + (a2 + a3)`).
// Every function below is a stub that takes one fixed argument: replace each with a variadic template.

// ---- 1. How many? `sizeof...(Ts)` counts a pack. ---------------------------------------------------------------------
// TODO: count_of(1, "a", 2.5) is 3, count_of() is 0:   template <class... Ts> constexpr std::size_t count_of(const Ts&...)
constexpr std::size_t count_of(int) { return 1; }

// ---- 2. Fold expressions ---------------------------------------------------------------------------------------------
// A BINARY fold has an initial value, so it also works for an EMPTY pack:  (0 + ... + args)  is  ((0 + a1) + a2) + a3, and 0 for no arguments.
// A unary fold (`(... + args)`) is an error for an empty pack, except for && (true), || (false) and the comma operator.
// TODO: sum(1, 2, 3) is 6, sum() is 0, sum(1, 2.5) is 3.5 (the common type of the arguments). constexpr.
constexpr int sum(int a) { return a; }

// TODO: all_true(args...): every argument is true; an empty pack is true.   `(... && args)`
constexpr bool all_true(bool a) { return a; }
// TODO: any_true(args...): at least one is true; an empty pack is false.   `(... || args)`
constexpr bool any_true(bool a) { return a; }

// TODO: all_equal(first, rest...): every following argument equals `first` (they may have different types). The pattern before
// the `...` is repeated for each argument:  ((first == rest) && ...)
constexpr bool all_equal(int first) {
    (void)first;
    return true;
}

// ---- 3. A type-safe printf ---------------------------------------------------------------------------------------------
// TODO: interpolate("{} + {} = {}", 1, 2, 3) is "1 + 2 = 3". Each `{}` is replaced by the next argument written with `<<`, so any printable
// type works. More arguments than `{}`: the extra ones are ignored. More `{}` than arguments: the rest stay as `{}`.
// Hint: a lambda `put(arg)` that finds the next "{}" in the format string (`fmt.find("{}", pos)`), writes the text before it and then
// `arg`, and moves `pos` past it, and then a fold over the COMMA operator that calls it for every argument, in order:  (put(args), ...);
// Finally write whatever is left of the format string.
inline std::string interpolate(std::string_view fmt) { return std::string(fmt); }

// ---- 4. Perfect forwarding ---------------------------------------------------------------------------------------------
// TODO: make_owned<T>(args...) builds a T from any arguments: `new T(std::forward<Args>(args)...)` with `Args&&... args` (forwarding references).
// std::forward<Args>(args) passes each argument on with the SAME value category: lvalues stay lvalues (copied), rvalues stay rvalues (moved).
// Without std::forward every argument is an lvalue (it has a name), so everything would be copied.
template <class T>
std::unique_ptr<T> make_owned() {
    return std::unique_ptr<T>(new T());
}

// TODO: for_each_arg(f, args...) calls f on every argument in order, forwarding each:  (f(std::forward<Args>(args)), ...);
template <class F>
void for_each_arg(F&& f) {
    (void)f;
}
