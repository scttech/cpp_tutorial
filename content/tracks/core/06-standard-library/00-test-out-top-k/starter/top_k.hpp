#pragma once

#include <algorithm>
#include <cctype>
#include <cstddef>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <vector>

// The k most frequent words of a text.
//   - a word is a maximal run of ASCII letters; everything else (digits, spaces, punctuation, apostrophes, bytes >= 128) separates words
//   - words are compared case-insensitively and reported in lower case
//   - the result is ordered by count, highest first; words with the same count are in alphabetical order
//   - fewer than k words if the text has fewer distinct words; k == 0 gives an empty result
//
// Use the standard library: a container to count, an algorithm to pick the top k. (The Fast tier counts heap allocations.)
inline std::vector<std::string> top_k_words(std::string_view text, std::size_t k) {
    (void)text;
    (void)k;
    return {};
}
