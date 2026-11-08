#pragma once

#include <functional>
#include <iterator>
#include <utility>

// The element of `range` whose key is the largest, where the key of an element is `proj(element)`.
// Returns an ITERATOR to it (the FIRST of several equal ones), or end() for an empty range.
//   max_by(people, &Person::age)                      a member pointer is a projection
//   max_by(words, [](const std::string& w) { return w.size(); })
//   max_by(v, [](int x) { return x; }, std::greater<>{})   an optional comparison of the keys: this finds the minimum
//
// What the signature needs: a template over the range type (a forwarding reference `Range&&` accepts a vector, a const
// list, a C array ...), over the projection, and over an OPTIONAL comparison with a default (`std::less<>`).
// Hints: std::begin / std::end work on every range; std::invoke(proj, x) calls a lambda, a function AND a member pointer;
// compute each key once; use only `comp(best_key, key)` to decide (never `>` or `==` on the keys).
//
// As given, the function takes only two parameters and always returns the first element. Rewrite it.
template <class Range, class Proj>
auto max_by(Range&& range, Proj proj) {
    (void)proj;
    return std::begin(range);
}
