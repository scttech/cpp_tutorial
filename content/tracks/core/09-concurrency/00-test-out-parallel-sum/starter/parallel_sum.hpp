#pragma once

#include <algorithm>
#include <cstddef>
#include <numeric>
#include <stdexcept>
#include <thread>
#include <vector>

// TEST-OUT: add up a big vector with several threads.
//
//   long long parallel_sum(const std::vector<int>& values, unsigned thread_count);
//
//   * The result is the sum of all the values. Split the vector into contiguous parts, give each part to a thread of its own, wait for all of
//     the threads, and add up what they found.
//   * thread_count == 0 is a mistake: std::invalid_argument.
//   * Never more threads than there are values (an empty vector needs no thread at all, and 3 values never need 1000 threads).
//   * The int values can add up to more than INT_MAX: the result is a long long.
//   * Every thread is finished (joined) when the function returns. The input is not modified.
//
// Your program runs under ThreadSanitizer: two threads that touch the same variable without coordination are reported, even if the answer comes out right.
inline long long parallel_sum(const std::vector<int>& values, unsigned thread_count) {
    (void)values;
    (void)thread_count;
    return 0;
}
