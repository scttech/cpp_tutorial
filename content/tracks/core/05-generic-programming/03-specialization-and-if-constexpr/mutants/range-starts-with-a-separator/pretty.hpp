// MUTANT: [, 1, 2]: the separator is written before every element.
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
// Part 1: TypeName<T>. A CLASS template can be SPECIALIZED: a different definition for particular types.
// ==================================================================================================================

// The primary template: used when no specialization matches.
template <class T>
struct TypeName {
    static std::string name() { return "unknown"; }
};

// FULL specializations: one exact type each (`template <>`: no parameters left).
template <>
struct TypeName<int> {
    static std::string name() { return "int"; }
};
template <>
struct TypeName<double> {
    static std::string name() { return "double"; }
};
template <>
struct TypeName<bool> {
    static std::string name() { return "bool"; }
};
template <>
struct TypeName<std::string> {
    static std::string name() { return "string"; }
};

// PARTIAL specializations: a whole family of types that share a shape. TypeName<int*>, TypeName<double*> ... match T*.
template <class T>
struct TypeName<T*> {
    static std::string name() { return "pointer to " + TypeName<T>::name(); }
};
template <class T>
struct TypeName<const T> {
    static std::string name() { return "const " + TypeName<T>::name(); }
};
template <class T>
struct TypeName<std::vector<T>> {
    static std::string name() { return "vector of " + TypeName<T>::name(); }
};
template <class A, class B>
struct TypeName<std::pair<A, B>> {
    static std::string name() { return "pair of " + TypeName<A>::name() + " and " + TypeName<B>::name(); }
};

// ==================================================================================================================
// Part 2: to_text(x). A FUNCTION template cannot be partially specialized, so:
//   - one generic function that picks its behavior with `if constexpr`, and
//   - OVERLOADS for the shapes that need their own function (pair, optional).
// ==================================================================================================================

// These two are declared BEFORE the generic function on purpose. When the generic one prints the elements of a std::map
// (which are pairs), it calls to_text(element). That call depends on the type, so the compiler looks for candidates when it reads
// the template (only what is declared ABOVE it) and again at the call, through argument-dependent lookup, which only searches the
// namespaces of the arguments (std, for a pair): not the global namespace where these overloads live. Declared below, they would never be found.
template <class A, class B>
std::string to_text(const std::pair<A, B>& pair);
template <class T>
std::string to_text(const std::optional<T>& opt);

template <class T>
constexpr bool is_range = requires(const T& t) {
    std::begin(t);
    std::end(t);
};
template <class T>
constexpr bool is_streamable = requires(std::ostream& os, const T& t) { os << t; };

template <class T>
std::string to_text(const T& value) {
    // `if constexpr` decides at COMPILE time and throws the other branches away: they are not even compiled for this T.
    // (A plain `if` would compile every branch for every type, and `*value` for an int would be an error.)
    if constexpr (std::is_same_v<T, bool>) {
        return value ? "true" : "false";
    } else if constexpr (std::is_same_v<T, char>) {
        return std::string("'") + value + "'";
    } else if constexpr (std::is_integral_v<T>) {
        return std::to_string(value);
    } else if constexpr (std::is_floating_point_v<T>) {
        std::ostringstream os;
        os << value;
        return os.str();
    } else if constexpr (std::is_same_v<T, std::nullptr_t>) {
        return "nullptr";
    } else if constexpr (std::is_convertible_v<const T&, std::string_view>) {  // std::string, string_view, const char*, "literal"
        return "\"" + std::string(std::string_view(value)) + "\"";
    } else if constexpr (std::is_pointer_v<T>) {
        return value == nullptr ? "nullptr" : "&" + to_text(*value);
    } else if constexpr (is_range<T>) {
        std::string out = "[";
        bool first = true;
        for (const auto& element : value) {
            out += ", ";
            first = false;
            out += to_text(element);
        }
        return out + "]";
    } else if constexpr (is_streamable<T>) {
        std::ostringstream os;
        os << value;
        return os.str();
    } else {
        return "<unprintable>";
    }
}

template <class A, class B>
std::string to_text(const std::pair<A, B>& pair) {
    return "(" + to_text(pair.first) + ", " + to_text(pair.second) + ")";
}

template <class T>
std::string to_text(const std::optional<T>& opt) {
    return opt ? "some(" + to_text(*opt) + ")" : "none";
}
