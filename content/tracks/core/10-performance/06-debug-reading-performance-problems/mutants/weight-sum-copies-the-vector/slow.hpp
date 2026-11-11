// MUTANT: the helper takes the vector by value again.
#pragma once

#include <algorithm>
#include <cstddef>
#include <map>
#include <string>
#include <vector>

#include "probe.hpp"

// 1. The elements of `v` in reverse order.
inline std::vector<Tracked> reversed(const std::vector<Tracked>& v) {
    return std::vector<Tracked>(v.rbegin(), v.rend());  // built once, from the end to the start
}

// 2. The sum of all the values.
inline int weight_at(std::vector<Tracked> all, std::size_t i) { return all[i].v; }

inline long long weight_sum(const std::vector<Tracked>& v) {
    long long sum = 0;
    for (std::size_t i = 0; i < v.size(); ++i) sum += weight_at(v, i);
    return sum;
}

// 3. The words, one after the other, with `sep` between them (and not after the last one).
inline std::string join(const std::vector<std::string>& words, char sep) {
    std::size_t total = words.empty() ? 0 : words.size() - 1;
    for (const std::string& w : words) total += w.size();
    std::string result;
    result.reserve(total);  // one allocation
    for (std::size_t i = 0; i < words.size(); ++i) {
        if (i > 0) result += sep;
        result += words[i];
    }
    return result;
}

// 4. Does any value occur twice?
inline bool has_duplicates(const std::vector<Tracked>& v) {
    std::vector<Tracked> sorted = v;  // equal values end up next to each other
    std::sort(sorted.begin(), sorted.end());
    return std::adjacent_find(sorted.begin(), sorted.end()) != sorted.end();
}

// 5. The value stored for `key`, or 0 when there is none.
inline int lookup_count(const std::map<Tracked, int>& table, const Tracked& key) {
    const auto it = table.find(key);  // one search
    return it == table.end() ? 0 : it->second;
}

// 6. The longest word (the FIRST of the longest ones), or "" when there are no words.
inline std::string longest(const std::vector<std::string>& words) {
    const std::string* best = nullptr;
    for (const std::string& word : words) {
        if (best == nullptr || word.size() > best->size()) best = &word;
    }
    return best == nullptr ? std::string() : *best;
}
