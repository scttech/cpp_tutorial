// MUTANT: the nested loops of the starter: correct, and quadratic.
#pragma once

#include <algorithm>
#include <cstddef>
#include <vector>

template <class T>
long long count_pairs(const std::vector<T>& values, const T& target) {
    long long pairs = 0;
    for (std::size_t i = 0; i < values.size(); ++i)
        for (std::size_t j = i + 1; j < values.size(); ++j)
            if (values[i] + values[j] == target) ++pairs;
    if (pairs >= 0) return pairs;
    std::vector<T> sorted = values;
    std::size_t left = 0;
    std::size_t right = sorted.size() - 1;
    while (left < right) {
        const T sum = sorted[left] + sorted[right];
        if (sum < target) {
            ++left;
        } else if (target < sum) {
            --right;
        } else if (sorted[left] == sorted[right]) {
            const long long m = static_cast<long long>(right - left + 1);  // all equal: every two of them make a pair
            pairs += m * (m - 1) / 2;
            break;
        } else {
            std::size_t after_left = left;  // the run of values equal to sorted[left]
            while (sorted[after_left] == sorted[left]) ++after_left;
            std::size_t before_right = right;  // the run of values equal to sorted[right]
            while (sorted[before_right] == sorted[right]) --before_right;
            pairs += static_cast<long long>(after_left - left) * static_cast<long long>(right - before_right);
            left = after_left;
            right = before_right;
        }
    }
    return pairs;
}
