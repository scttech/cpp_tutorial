// MUTANT: taken by value.
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

// ---- 1. How many? `sizeof...` counts a pack. --------------------------------------------------------------------------
template <class... Ts>
constexpr std::size_t count_of(const Ts&...) {
    return sizeof...(Ts);
}

// ---- 2. Fold expressions ----------------------------------------------------------------------------------------------
// A BINARY fold `(0 + ... + args)` has an initial value, so it also works for an EMPTY pack (the result is the 0).
// (A fold without one, `(... + args)`, is an error for an empty pack, except for &&, || and , which have a natural empty value.)
template <class... Ts>
constexpr auto sum(const Ts&... args) {
    return (0 + ... + args);  // ((0 + a1) + a2) + a3: the type is the common type of the arguments (sum(1, 2.5) is a double)
}

template <class... Ts>
constexpr bool all_true(const Ts&... args) {
    return (... && args);  // an empty && fold is true
}

template <class... Ts>
constexpr bool any_true(const Ts&... args) {
    return (... || args);  // an empty || fold is false
}

// True if every following argument equals `first`. The pattern before `...` is repeated for each element of the pack.
template <class T, class... Ts>
constexpr bool all_equal(const T& first, const Ts&... rest) {
    return ((first == rest) && ...);
}

// ---- 3. A type-safe printf: interpolate("{} + {} = {}", 1, 2, 3) is "1 + 2 = 3" -------------------------------------------------
// Each `{}` is replaced by the next argument, written with `<<`, so ANY printable type works (and there is no %d to get wrong).
// More arguments than `{}`: the extra ones are ignored. More `{}` than arguments: the rest stay as `{}`.
template <class... Args>
std::string interpolate(std::string_view fmt, Args... args) {
    std::ostringstream os;
    std::size_t pos = 0;
    auto put = [&](const auto& arg) {
        const std::size_t brace = fmt.find("{}", pos);
        if (brace == std::string_view::npos) return;  // no placeholder left: ignore this argument
        os << fmt.substr(pos, brace - pos) << arg;
        pos = brace + 2;
    };
    (put(args), ...);  // a fold over the COMMA operator: call `put` for each argument, in order
    os << fmt.substr(pos);
    return os.str();
}

// ---- 4. Perfect forwarding --------------------------------------------------------------------------------------------
// `Args&&...` with Args deduced is a pack of FORWARDING references: each binds to an lvalue (Args = X&) or an rvalue (Args = X).
// `std::forward<Args>(args)...` hands each one on with the SAME value category: lvalues stay lvalues (copied), rvalues stay
// rvalues (moved). Without std::forward every argument is an lvalue (it has a name), so everything would be copied.
template <class T, class... Args>
std::unique_ptr<T> make_owned(Args&&... args) {
    return std::unique_ptr<T>(new T(std::forward<Args>(args)...));
}

// Calls f on every argument, in order, forwarding each one: f(a1), f(a2), f(a3).
template <class F, class... Args>
void for_each_arg(F&& f, Args&&... args) {
    (f(std::forward<Args>(args)), ...);
}
