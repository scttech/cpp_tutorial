// MUTANT: <= 100.
#pragma once

#include <algorithm>
#include <concepts>
#include <cstddef>
#include <iterator>
#include <ranges>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

// Iterators are the glue between containers and algorithms; C++20 ranges put a pipeline syntax on top of them.

// ---- 1. Iterator categories ----------------------------------------------------------------------------------------------
// What can this iterator do? From the most capable to the least: contiguous (a pointer to an array: vector, array, string), random access
// (jump by n in O(1): deque), bidirectional (++ and --: list, map), forward (++ only, can be copied and replayed: forward_list), input
// (single pass: istream_iterator), output (write only: back_inserter, ostream_iterator).
// Returns one of "contiguous", "random access", "bidirectional", "forward", "input", "output" (or "none").
// Test the MOST capable first: a contiguous iterator is also random access, bidirectional, forward and input.
template <class It>
constexpr const char* category_name() {
    if constexpr (std::contiguous_iterator<It>) {
        return "contiguous";
    } else if constexpr (std::random_access_iterator<It>) {
        return "random access";
    } else if constexpr (std::bidirectional_iterator<It>) {
        return "bidirectional";
    } else if constexpr (std::forward_iterator<It>) {
        return "forward";
    } else if constexpr (std::input_iterator<It>) {
        return "input";
    } else if constexpr (std::output_iterator<It, int>) {
        return "output";
    } else {
        return "none";
    }
}

// ---- 2. Algorithms choose their strategy by category: std::advance, written by hand ------------------------------------------------
// Moves `it` forward by n places (backward when n is negative, for bidirectional iterators).
// A random access iterator does it in ONE jump (`it += n`); a bidirectional one steps n times with ++ (or --); a forward/input one steps
// with ++ (n must not be negative).
template <class It>
void advance_by(It& it, std::ptrdiff_t n) {
    if constexpr (std::random_access_iterator<It>) {
        it += n;
    } else if constexpr (std::bidirectional_iterator<It>) {
        for (; n > 0; --n) ++it;
        for (; n < 0; ++n) --it;
    } else {
        for (; n > 0; --n) ++it;
    }
}

// ---- 3. Ranges: views are LAZY, composable pipelines -------------------------------------------------------------------------------
// The squares of the even numbers, in order:  squares_of_evens({1, 2, 3, 4}) is {4, 16}.
// Write it as ONE pipeline: values | std::views::filter(...) | std::views::transform(...), then copy the result into a vector.
inline std::vector<int> squares_of_evens(const std::vector<int>& values) {
    auto pipeline = values | std::views::filter([](int x) { return x % 2 == 0; }) | std::views::transform([](int x) { return x * x; });
    std::vector<int> result;
    std::ranges::copy(pipeline, std::back_inserter(result));
    return result;
}

// Skips the leading negative numbers, then takes values while they are below 100:
//   middle_section({-3, -1, 5, 7, 120, 9}) is {5, 7}   (the 9 after the 120 is not taken: take_while stops at the first failure)
inline std::vector<int> middle_section(const std::vector<int>& values) {
    auto pipeline = values | std::views::drop_while([](int x) { return x < 0; }) | std::views::take_while([](int x) { return x <= 100; });
    std::vector<int> result;
    std::ranges::copy(pipeline, std::back_inserter(result));
    return result;
}

// Splits the text at every `separator`, keeping empty pieces:  split_pieces("a,,b", ',') is {"a", "", "b"}; split_pieces("", ',') is {} (an empty text has no pieces: that is how std::views::split behaves).
// (std::views::split gives subranges: build a std::string from each.)
inline std::vector<std::string> split_pieces(std::string_view text, char separator) {
    std::vector<std::string> pieces;
    for (const auto& piece : text | std::views::split(separator)) pieces.emplace_back(piece.begin(), piece.end());
    return pieces;
}

// The first n non-negative integers that satisfy `pred`, in order. The integers are an INFINITE view (std::views::iota(0)): the pipeline
// is lazy, so `pred` is called only until n matches were found, not for every integer.
template <class Pred>
std::vector<int> first_n_matching(Pred pred, std::size_t n) {
    auto pipeline = std::views::iota(0) | std::views::filter(pred) | std::views::take(n);
    std::vector<int> result;
    for (int x : pipeline) result.push_back(x);
    return result;
}

// ---- 4. Iterator invalidation ---------------------------------------------------------------------------------------------------
// Every element appears twice in a row:  duplicate_each({1, 2, 3}) makes {1, 1, 2, 2, 3, 3}. Must handle 400000 elements.
// Inserting into a vector while iterating over it invalidates the iterators (it may reallocate), and inserting one element at a time in the middle
// is O(n) each. Build the new contents in one pass instead.
inline void duplicate_each(std::vector<int>& values) {
    std::vector<int> result;
    result.reserve(values.size() * 2);
    for (int x : values) {
        result.push_back(x);
        result.push_back(x);
    }
    values = std::move(result);
}
