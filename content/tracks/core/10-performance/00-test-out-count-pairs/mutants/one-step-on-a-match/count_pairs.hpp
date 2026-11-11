// MUTANT: after a match only one pointer moves: pairs are counted twice or lost.
#pragma once

#include <algorithm>
#include <cstddef>
#include <vector>

template <class T>
long long count_pairs(const std::vector<T>& values, const T& target) {
    if (values.size() < 2) return 0;
    std::vector<T> sorted = values;  // sort a COPY: the caller's vector must not change
    std::sort(sorted.begin(), sorted.end());

    long long pairs = 0;
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
            ++left;
        }
    }
    return pairs;
}
