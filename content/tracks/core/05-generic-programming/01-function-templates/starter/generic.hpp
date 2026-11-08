#pragma once

#include <initializer_list>
#include <type_traits>
#include <utility>

// Three small generic functions. As written they work for `int` only: make them templates.
// (A template is a RECIPE: the compiler writes a real function for each combination of types you use. That is why
// templates live in HEADERS, and why a function defined in a header is `inline` or a template.)

// ---- 1. clamp_value --------------------------------------------------------------------------------------------------
// TODO: the value forced into the range [low, high] (low if value < low, high if high < value, else value), for ANY type
// that has operator<. One type parameter T: `template <class T> constexpr const T& clamp_value(const T& value, const T& low, const T& high)`.
// It returns a REFERENCE to one of its arguments (no copy). Use only `<`, never `>`.
inline int clamp_value(int value, int low, int high) { return value < low ? low : (value > high ? high : value); }

// ---- 2. min_of -------------------------------------------------------------------------------------------------
// TODO: the smaller of two values that may have DIFFERENT types (an int and a double): two type parameters A and B,
// and the result type `std::common_type_t<A, B>` (int and double give double). The first one wins when they are equal.
inline int min_of(int a, int b) { return b < a ? b : a; }

// TODO: also the smallest of a braced list, `min_of({3, 1, 2})`: a parameter `std::initializer_list<T> values` lets the
// compiler deduce T from the elements. (You may assume the list is not empty.)

// ---- 3. swap_values --------------------------------------------------------------------------------------------
// TODO: exchange two objects of any type T by MOVING them. Better: write `using std::swap; swap(a, b);`: the unqualified
// call also finds a `swap` that the type itself provides (argument-dependent lookup), which `std::swap(a, b)` would ignore.
inline void swap_values(int& a, int& b) {
    int tmp = a;
    a = b;
    b = tmp;
}
