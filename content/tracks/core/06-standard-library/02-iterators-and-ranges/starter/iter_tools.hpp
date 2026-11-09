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
// Every function is a stub that compiles but is wrong: replace each body.

// ---- 1. Iterator categories ----------------------------------------------------------------------------------------------
// What can this iterator do? From the most capable to the least: contiguous (a pointer to an array: vector, array, string), random access
// (jump by n in O(1): deque), bidirectional (++ and --: list, map), forward (++ only, can be copied and replayed: forward_list), input
// (single pass: istream_iterator), output (write only: back_inserter, ostream_iterator).
// TODO: return one of "contiguous", "random access", "bidirectional", "forward", "input", "output" (or "none").
// Use the C++20 concepts std::contiguous_iterator<It>, std::random_access_iterator<It>, std::bidirectional_iterator<It>, std::forward_iterator<It>,
// std::input_iterator<It>, std::output_iterator<It, int> in an `if constexpr` chain. Test the MOST capable first: a contiguous iterator is also
// random access, bidirectional, forward and input.
template <class It>
constexpr const char* category_name() {
    return "none";
}

// ---- 2. Algorithms choose their strategy by category: std::advance, written by hand ------------------------------------------------
// TODO: move `it` forward by n places (backward when n is negative, for bidirectional iterators).
// A random access iterator does it in ONE jump (`it += n`); a bidirectional one steps n times with ++ (or --); a forward/input one steps
// with ++ (n must not be negative).
template <class It>
void advance_by(It& it, std::ptrdiff_t n) {
    (void)it;
    (void)n;
}

// ---- 3. Ranges: views are LAZY, composable pipelines -------------------------------------------------------------------------------
// TODO: the squares of the even numbers, in order:  squares_of_evens({1, 2, 3, 4}) is {4, 16}.
// Write it as ONE pipeline: values | std::views::filter(...) | std::views::transform(...), then copy the result into a vector
// (std::ranges::copy(pipeline, std::back_inserter(result))).
inline std::vector<int> squares_of_evens(const std::vector<int>& values) {
    (void)values;
    return {};
}

// TODO: skip the leading negative numbers (std::views::drop_while), then take values while they are below 100 (std::views::take_while):
//   middle_section({-3, -1, 5, 7, 120, 9}) is {5, 7}   (the 9 after the 120 is not taken: take_while stops at the first failure)
inline std::vector<int> middle_section(const std::vector<int>& values) {
    (void)values;
    return {};
}

// TODO: split the text at every `separator`, keeping empty pieces:  split_pieces("a,,b", ',') is {"a", "", "b"}; split_pieces("", ',') is {} (an empty text has no pieces: that is how std::views::split behaves).
// std::views::split(separator) gives subranges: build a std::string from each one (piece.begin(), piece.end()).
inline std::vector<std::string> split_pieces(std::string_view text, char separator) {
    (void)text;
    (void)separator;
    return {};
}

// TODO: the first n non-negative integers that satisfy `pred`, in order. The integers are an INFINITE view (std::views::iota(0)): the pipeline
// is lazy, so `pred` is called only until n matches were found, not for every integer.   iota(0) | filter(pred) | take(n)
template <class Pred>
std::vector<int> first_n_matching(Pred pred, std::size_t n) {
    (void)pred;
    (void)n;
    return {};
}

// ---- 4. Iterator invalidation ---------------------------------------------------------------------------------------------------
// TODO: every element appears twice in a row:  duplicate_each({1, 2, 3}) makes {1, 1, 2, 2, 3, 3}. Must handle 400000 elements.
// Inserting into a vector while iterating over it invalidates the iterators (it may reallocate), and inserting one element at a time in the middle
// is O(n) each. Build the new contents in one pass instead.
inline void duplicate_each(std::vector<int>& values) {
    (void)values;
}
