#pragma once

#include <algorithm>
#include <cstddef>
#include <vector>

// TEST-OUT: Make it fast.
//
//   long long count_pairs(const std::vector<T>& values, const T& target);
//
// How many pairs of positions i < j have values[i] + values[j] == target?  (Two equal values at different positions are a pair.)
//
// The version below is CORRECT but slow: it tries every pair. Make it fast, without changing the answers:
//   * Works / Robust: the right count for every input (empty, one value, many equal values, negative values ...). The vector must not be changed.
//   * Fast: the work grows like n log n and not like n squared. The tests count the operations T is asked to do (==, <, +, copies); the numbers
//     are shown by main.cpp. A T needs only ==, <, + and copying.
template <class T>
long long count_pairs(const std::vector<T>& values, const T& target) {
    long long pairs = 0;
    for (std::size_t i = 0; i < values.size(); ++i) {
        for (std::size_t j = i + 1; j < values.size(); ++j) {
            if (values[i] + values[j] == target) ++pairs;
        }
    }
    return pairs;
}
