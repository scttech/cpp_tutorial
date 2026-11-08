#pragma once

#include <concepts>
#include <cstddef>
#include <ostream>
#include <sstream>
#include <string>
#include <type_traits>

// A CONCEPT is a named requirement on a template argument: a compile-time yes/no question about a type.
// Using one turns "a screen of errors from inside the template" into "this type does not satisfy Number".
// Everything below compiles, but no constraint is written yet: every concept says yes to everything. Fill them in, and put the
// constraints on the five templates.

// ---- Concepts ------------------------------------------------------------------------------------------------------
// Build them from the standard ones (std::integral, std::floating_point, std::same_as, std::convertible_to) with && , || and !.

// TODO: Integer: an integral type, but NOT bool (bool is integral too): std::integral<T> && !std::same_as<T, bool>
template <class T>
concept Integer = true;

// TODO: Number: an Integer or a floating-point type. Define it IN TERMS OF Integer (`Integer<T> || ...`): that is what lets the compiler see
// that every Integer is a Number ("Integer subsumes Number"), so that an overload for Integer beats an overload for Number.
template <class T>
concept Number = true;

// TODO: Printable: a requires-EXPRESSION asks "would this code compile?":
//     requires(std::ostream& os, const T& value) { os << value; }
template <class T>
concept Printable = true;

// TODO: Sequence: something like a standard container. It has a value_type (`typename C::value_type;`), `c.begin();`, `c.end();`
// and a size() that converts to std::size_t (`{ c.size() } -> std::convertible_to<std::size_t>;`).
//     template <class C> concept Sequence = requires(const C& c) { ... };
template <class C>
concept Sequence = true;

// ---- 1. A requires CLAUSE after the template header ----------------------------------------------------------------
// TODO: only for a Sequence whose elements are Numbers:   template <Sequence S> requires Number<typename S::value_type>
// The mean as a double; the mean of an empty sequence is 0.0 (not 0/0).
template <class S>
double mean(const S& values) {
    (void)values;
    return 0.0;
}

// ---- 2. Constrained parameters (the shortest form) -----------------------------------------------------------------
// TODO: both parameters must be Numbers: `auto larger(Number auto a, Number auto b)`. The result is the larger one, with the
// common type of the two (the plain `a < b ? b : a` does that). larger(1, 2.5) is 2.5.
auto larger(auto a, auto b) {
    (void)b;
    return a;
}

// ---- 3. Overloads that differ ONLY by constraint --------------------------------------------------------------------
// describe(x) says "something" for any type, "a number" for a Number, "an integer" for an Integer. The most constrained overload that fits is chosen.
// TODO: add the two constrained overloads (`template <Number T>` and `template <Integer T>`), each taking `const T&`.
template <class T>
std::string describe(const T&) {
    return "something";
}

// ---- 4. A constrained class template -------------------------------------------------------------------------------
// TODO: only for Numbers (`template <Number T>`): add(value), total(), count(), and mean() (0.0 when nothing was added).
template <class T>
class Accumulator {
public:
    void add(T value) { (void)value; }
    T total() const { return T{}; }
    std::size_t count() const { return 0; }
    double mean() const { return 0.0; }
};

// ---- 5. Constrain what the body needs -------------------------------------------------------------------------------
// TODO: the elements must be Printable: `template <Sequence S> requires Printable<typename S::value_type>`.
// join({1, 2, 3}, ", ") is "1, 2, 3": the elements separated by `separator`, no separator at the end.
template <class S>
std::string join(const S& values, const std::string& separator) {
    (void)values;
    (void)separator;
    return "";
}
