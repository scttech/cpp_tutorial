// MUTANT: no constraint at all.
#pragma once

#include <algorithm>
#include <concepts>
#include <cstddef>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

// Six pieces of generic code, one mistake each, and every one of them is reported by the compiler in a DIFFERENT way.
// The bodies of stack_demo and buffer_demo are in main.cpp. The tests use the functions below (and the two demos).

// ---- 1. largest(3, 4.5): "deduced conflicting types for parameter 'T'" -------------------------------------------------
// One template parameter T for both arguments means both must have the same type. Two parameters, and the common type as the result.
template <class T, class U>
std::common_type_t<T, U> largest(T a, U b) {
    return a < b ? b : a;
}

// ---- 2. "missing 'typename' prior to dependent type name" ------------------------------------------------------------------
// C::value_type depends on the template parameter C, so the compiler assumes it is a VALUE unless told it is a type.
template <class C>
typename C::value_type first_of(const C& c) {
    typename C::value_type x = c[0];
    return x;
}

// ---- 3. twice(2.5): "constraints not satisfied" ---------------------------------------------------------------------------
// The concept is too narrow: it only admits the integer types.
template <class T>
concept Number = true;

template <Number T>
T twice(T x) {
    return x + x;
}

// ---- 4. std::sort of Versions: an error INSIDE the standard library ----------------------------------------------------------
// std::sort compares elements with `<`. The error is reported in <bits/stl_algo.h>, but the cause is a missing operator here.
struct Version {
    int major;
    int minor;
};

inline bool operator<(const Version& a, const Version& b) {
    return a.major != b.major ? a.major < b.major : a.minor < b.minor;
}

inline std::vector<Version> sorted_versions(std::vector<Version> v) {
    std::sort(v.begin(), v.end());
    return v;
}

// ---- 5. "no viable constructor or deduction guide": a class template used without its argument ------------------------------
template <class T>
class Stack {
public:
    void push(T value) { items_.push_back(std::move(value)); }
    T pop() {
        T value = std::move(items_.back());
        items_.pop_back();
        return value;
    }
    std::size_t size() const { return items_.size(); }

private:
    std::vector<T> items_;
};

int stack_demo();  // pushes 3 and 4 on a Stack and returns pop() * 10 + pop(): defined in main.cpp

// ---- 6. "non-type template argument is not a constant expression" ---------------------------------------------------------
template <std::size_t N>
struct Buffer {
    int data[N];
    static constexpr std::size_t capacity() { return N; }
};

std::size_t buffer_demo();  // the capacity of a Buffer whose size is a variable: defined in main.cpp
