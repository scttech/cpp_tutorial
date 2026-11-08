#pragma once

#include <cstddef>
#include <iterator>
#include <optional>
#include <ostream>
#include <sstream>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>
#include <vector>

// ==================================================================================================================
// Part 1: TypeName<T>::name() describes a type in words. A CLASS template can be SPECIALIZED: given a different
// definition for particular types.
// ==================================================================================================================

// The primary template: used when no specialization matches.
template <class T>
struct TypeName {
    static std::string name() { return "unknown"; }
};

// TODO: FULL specializations (`template <>`) for int ("int"), double ("double"), bool ("bool") and std::string ("string"):
//     template <>
//     struct TypeName<int> {
//         static std::string name() { return "int"; }
//     };
//
// TODO: PARTIAL specializations: families of types that share a shape (they keep some template parameters):
//     template <class T> struct TypeName<T*>                  name() = "pointer to " + TypeName<T>::name()      TypeName<int*>: "pointer to int"
//     template <class T> struct TypeName<const T>             "const " + ...                                    TypeName<const int>: "const int"
//     template <class T> struct TypeName<std::vector<T>>      "vector of " + ...                                TypeName<std::vector<int*>>: "vector of pointer to int"
//     template <class A, class B> struct TypeName<std::pair<A, B>>   "pair of " + A's name + " and " + B's name

// ==================================================================================================================
// Part 2: to_text(x) turns a value of (almost) any type into a readable string. A FUNCTION template cannot be partially
// specialized, so use one generic function with `if constexpr`, plus OVERLOADS for shapes that need their own function.
//
//   bool                   true / false              char                    'a'
//   other integers         42, -7, 65 (a signed or unsigned char is a number)
//   floating point         1.5, 2, 0.1 (what `os << x` prints)
//   strings                "hello" in double quotes: std::string, std::string_view, const char*, string literals
//   std::nullptr_t         nullptr                   a pointer              nullptr, or & followed by the pointed-to value: &5
//   std::pair<A, B>        (1, "a")                  std::optional<T>        none, or some(5)
//   any range              [1, 2, 3]  [] for empty; nested ranges and elements that are pairs or optionals work too
//   a type with an operator<<   whatever that prints          anything else    <unprintable>
// ==================================================================================================================

// TODO: declare the overloads for std::pair and std::optional HERE, before the generic function. When the generic function prints
// the elements of a std::map (pairs), it calls to_text(element); a dependent call only finds overloads that are declared above it
// (or in the namespaces of the arguments, which for std::pair is std, not the global namespace).
//     template <class A, class B> std::string to_text(const std::pair<A, B>& pair);

// TODO: write the generic function with an `if constexpr` chain: std::is_same_v<T, bool>, std::is_integral_v<T>,
// std::is_floating_point_v<T>, std::is_convertible_v<const T&, std::string_view>, std::is_pointer_v<T>, a range
// (`requires(const T& t) { std::begin(t); std::end(t); }`), a type that can be streamed (`os << t`), and the fallback. Mind the order:
// bool and char are integers too, and a string is a range of chars.
template <class T>
std::string to_text(const T& value) {
    (void)value;
    return "?";
}

// TODO: the overloads for std::pair and std::optional, defined here (declared above).
