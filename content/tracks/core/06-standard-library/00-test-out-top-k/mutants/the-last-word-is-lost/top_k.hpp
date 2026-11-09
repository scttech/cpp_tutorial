// MUTANT: the loop stops one character early.
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
inline std::vector<std::string> top_k_words(std::string_view text, std::size_t k) {
    auto is_letter = [](char c) { return std::isalpha(static_cast<unsigned char>(c)) != 0; };

    std::string lower(text);  // ONE copy of the text, lower-cased: the words below are views into it (no string per word)
    for (char& c : lower) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));

    std::unordered_map<std::string_view, std::size_t> counts;
    const std::string_view all(lower);
    std::size_t i = 0;
    while (i + 1 < all.size()) {
        if (!is_letter(all[i])) {
            ++i;
            continue;
        }
        std::size_t j = i;
        while (j < all.size() && is_letter(all[j])) ++j;
        ++counts[all.substr(i, j - i)];
        i = j;
    }

    std::vector<std::pair<std::string_view, std::size_t>> items(counts.begin(), counts.end());
    const auto better = [](const auto& a, const auto& b) { return a.second != b.second ? a.second > b.second : a.first < b.first; };
    k = std::min(k, items.size());
    std::partial_sort(items.begin(), items.begin() + static_cast<std::ptrdiff_t>(k), items.end(), better);  // only the first k need to be in order

    std::vector<std::string> result;
    result.reserve(k);
    for (std::size_t n = 0; n < k; ++n) result.emplace_back(items[n].first);
    return result;
}
