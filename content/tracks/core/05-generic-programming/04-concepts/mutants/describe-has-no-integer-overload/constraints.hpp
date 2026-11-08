// MUTANT: integers are only 'a number'.
#pragma once

#include <concepts>
#include <cstddef>
#include <ostream>
#include <sstream>
#include <string>
#include <type_traits>

// A CONCEPT is a named requirement on a template argument: a compile-time yes/no question about a type.
// Using one turns "a screen of errors from inside the template" into "this type does not satisfy Number".

// ---- Concepts ------------------------------------------------------------------------------------------------------
// Built from the standard ones (std::integral, std::floating_point, std::same_as) with && , || and !.
template <class T>
concept Integer = std::integral<T> && !std::same_as<T, bool>;  // bool is an integral type too: it is not an INTEGER here

// Defined IN TERMS OF Integer on purpose: that is what lets the compiler see that every Integer is a Number
// (Integer "subsumes" Number), so that an overload for Integer beats an overload for Number.
template <class T>
concept Number = Integer<T> || std::floating_point<T>;

// A requires-EXPRESSION asks "would this code compile?": here, can a const T be written to a std::ostream?
template <class T>
concept Printable = requires(std::ostream& os, const T& value) { os << value; };

// Something like a standard container: has value_type, begin(), end() and a size().
template <class C>
concept Sequence = requires(const C& c) {
    typename C::value_type;
    c.begin();
    c.end();
    { c.size() } -> std::convertible_to<std::size_t>;
};

// ---- 1. A requires CLAUSE after the template header ----------------------------------------------------------------
template <Sequence S>
    requires Number<typename S::value_type>
double mean(const S& values) {
    if (values.size() == 0) return 0.0;  // the mean of nothing: 0, not 0/0 = NaN
    double total = 0.0;
    for (const auto& v : values) total += static_cast<double>(v);
    return total / static_cast<double>(values.size());
}

// ---- 2. Constrained parameters (the shortest form): each `Number auto` is a template parameter that must be a Number ---
auto larger(Number auto a, Number auto b) {
    return a < b ? b : a;  // the common type of the two, like ?: always gives
}

// ---- 3. Overloads that differ ONLY by constraint: the most constrained one that fits is chosen ---------------------------
template <class T>
std::string describe(const T&) {
    return "something";
}
template <Number T>
std::string describe(const T&) {
    return "a number";
}

// ---- 4. A constrained class template ----------------------------------------------------------------------------------
template <Number T>
class Accumulator {
public:
    void add(T value) {
        total_ += value;
        ++count_;
    }
    T total() const { return total_; }
    std::size_t count() const { return count_; }
    double mean() const { return count_ == 0 ? 0.0 : static_cast<double>(total_) / static_cast<double>(count_); }

private:
    T total_{};
    std::size_t count_ = 0;
};

// ---- 5. Constrain what the body needs --------------------------------------------------------------------------------
template <Sequence S>
    requires Printable<typename S::value_type>
std::string join(const S& values, const std::string& separator) {
    std::ostringstream os;
    bool first = true;
    for (const auto& v : values) {
        if (!first) os << separator;
        first = false;
        os << v;
    }
    return os.str();
}
