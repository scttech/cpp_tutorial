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
// Range&& is a forwarding reference: it accepts a vector, a const list, an array, ... std::begin/std::end work on all of them.
// std::invoke calls ANY callable (a lambda, a function, a member pointer). The key is computed once per element.
template <class Range, class Proj, class Compare = std::less<>>
auto max_by(Range&& range, Proj proj, Compare comp = {}) {
    auto it = std::begin(range);
    const auto last = std::end(range);
    if (it == last) return it;

    auto best = it;
    auto best_key = std::invoke(proj, *it);  // `auto` copies the key: the element it came from may move on
    for (++it; it != last; ++it) {
        auto key = std::invoke(proj, *it);
        if (comp(best_key, key)) {  // strictly "better": equal keys keep the first element
            best = it;
            best_key = std::move(key);
        }
    }
    return best;
}
