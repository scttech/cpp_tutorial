#pragma once

#include <algorithm>
#include <concepts>
#include <cstddef>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

// Six pieces of generic code, one mistake each. The bodies of stack_demo and buffer_demo are in main.cpp.
// The tests use the functions below (and the two demos).

// Fix them one at a time, FIRST ERROR FIRST, and recompile after each fix. Each bug is a different kind of template error.

// ---- 1. largest
template <class T>
T largest(T a, T b) {
    return a < b ? b : a;
}

// ---- 2. first_of
template <class C>
typename C::value_type first_of(const C& c) {
    C::value_type x = c[0];
    return x;
}

// ---- 3. twice
template <class T>
concept Number = std::integral<T>;

template <Number T>
T twice(T x) {
    return x + x;
}

// ---- 4. sorted_versions
struct Version {
    int major;
    int minor;
};

inline std::vector<Version> sorted_versions(std::vector<Version> v) {
    std::sort(v.begin(), v.end());
    return v;
}

// ---- 5. Stack
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

// ---- 6. Buffer
template <std::size_t N>
struct Buffer {
    int data[N];
    static constexpr std::size_t capacity() { return N; }
};

std::size_t buffer_demo();  // the capacity of a Buffer whose size is a variable: defined in main.cpp
