#pragma once

#include <algorithm>
#include <cstddef>
#include <map>
#include <string>
#include <vector>

#include "probe.hpp"

// SIX functions. They all give the right answers, and all six are slower than they need to be, each in a different way.
// Run the program: main.cpp prints what each one costs on a realistic input (copies, moves, comparisons, heap allocations). Find the numbers that are out
// of proportion, work out WHY, and fix the cause WITHOUT changing any result. Fix the cause, not the symptom: the tests check each one separately.

// 1. The elements of `v` in reverse order.
inline std::vector<Tracked> reversed(const std::vector<Tracked>& v) {
    std::vector<Tracked> result;
    for (const Tracked& t : v) result.insert(result.begin(), t);
    return result;
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
    std::string result;
    for (std::size_t i = 0; i < words.size(); ++i) {
        if (i > 0) result = result + sep;
        result = result + words[i];
    }
    return result;
}

// 4. Does any value occur twice?
inline bool has_duplicates(const std::vector<Tracked>& v) {
    for (std::size_t i = 0; i < v.size(); ++i) {
        for (std::size_t j = i + 1; j < v.size(); ++j) {
            if (v[i] == v[j]) return true;
        }
    }
    return false;
}

// 5. The value stored for `key`, or 0 when there is none.
inline int lookup_count(const std::map<Tracked, int>& table, const Tracked& key) {
    if (table.count(key) > 0) return table.at(key);
    return 0;
}

// 6. The longest word (the FIRST of the longest ones), or "" when there are no words.
inline std::string longest(const std::vector<std::string>& words) {
    std::string best;
    for (std::string word : words) {
        if (word.size() > best.size()) best = word;
    }
    return best;
}
