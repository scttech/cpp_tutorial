// MUTANT: only the upper bound is enforced.
#pragma once

#include <initializer_list>
#include <type_traits>
#include <utility>

// Three small generic functions. A template is not a function but a RECIPE for functions: the compiler writes a real
// function for each combination of types you use. That is why templates live in HEADERS: the compiler needs the whole
// recipe in every file that uses it.

// ---- 1. clamp_value --------------------------------------------------------------------------------------------------
// The value forced into the range [low, high]. One type parameter T: all three arguments must have the SAME type, and T is
// deduced from them (clamp_value(5, 0, 3) -> T = int; clamp_value(5, 0, 3.5) is an error: int and double conflict).
// It returns a REFERENCE to one of its arguments (no copy), and needs only `operator<` from T.
template <class T>
constexpr const T& clamp_value(const T& value, const T& low, const T& high) {
    return high < value ? high : value;
}

// ---- 2. min_of -------------------------------------------------------------------------------------------------
// Two DIFFERENT types are fine here: A and B are deduced independently, and the result has their common type
// (int and double -> double), the same type the built-in `?:` would give.
template <class A, class B>
constexpr std::common_type_t<A, B> min_of(const A& a, const B& b) {
    return b < a ? b : a;  // equal: the first one
}

// The smallest of a braced list: min_of({3, 1, 2}). A parameter of type std::initializer_list<T> lets the compiler deduce T
// from the elements. (The list must not be empty.)
template <class T>
constexpr T min_of(std::initializer_list<T> values) {
    auto it = values.begin();
    T best = *it;
    for (++it; it != values.end(); ++it) {
        if (*it < best) best = *it;
    }
    return best;
}

// ---- 3. swap_values --------------------------------------------------------------------------------------------
// Exchanges two objects of any type, by MOVING (nothing is copied). The two lines are a pattern worth memorizing:
//   using std::swap;   makes std::swap a candidate (it is the fallback for types without their own swap)
//   swap(a, b);        an UNQUALIFIED call: argument-dependent lookup also finds a `swap` written for the type itself
// `std::swap(a, b)` would ignore a type's own, possibly faster, swap.
template <class T>
void swap_values(T& a, T& b) {
    using std::swap;
    swap(a, b);
}
