// MUTANT: the find_if predicate copies every record it looks at.
#pragma once

#include <algorithm>
#include <cctype>
#include <cstddef>
#include <iterator>
#include <numeric>
#include <optional>
#include <string>
#include <vector>

// A small data-cleaning pipeline built from the standard algorithms. Prefer a named algorithm to a hand-written loop:
// it says WHAT you do, it is usually correct on the edge cases, and it can be faster.

struct Record {
    std::string name;
    int age = 0;
    double score = 0.0;
};

// ---- 1. The erase-remove idiom -----------------------------------------------------------------------------------------------
// Removes the invalid records: an empty name, an age below 0 or above 150. The valid ones keep their order.
// std::remove_if does NOT shrink the vector (it moves the keepers to the front and returns the new end): you must erase the tail.
// Must handle 200000 records: erasing one at a time inside a loop is O(n) per erase.
inline void remove_invalid(std::vector<Record>& records) {
    const auto invalid = [](const Record& r) { return r.name.empty() || r.age < 0 || r.age > 150; };
    records.erase(std::remove_if(records.begin(), records.end(), invalid), records.end());
}

// ---- 2. std::transform --------------------------------------------------------------------------------------------------------
// Capitalizes each name: first letter upper case, the rest lower case ("aDA" -> "Ada"). ASCII letters only.
// (std::tolower and std::toupper need an unsigned char value.)
inline void normalize_names(std::vector<Record>& records) {
    std::for_each(records.begin(), records.end(), [](Record& r) {
        std::transform(r.name.begin(), r.name.end(), r.name.begin(), [](char c) { return static_cast<char>(std::tolower(static_cast<unsigned char>(c))); });
        if (!r.name.empty()) r.name[0] = static_cast<char>(std::toupper(static_cast<unsigned char>(r.name[0])));
    });
}

// ---- 3. std::sort with a comparator -------------------------------------------------------------------------------------------
// Highest score first; equal scores by name (A to Z); equal names by age (youngest first).
// The comparator must be a STRICT WEAK ORDERING: `<`-like (never true for equal elements), never `<=`.
inline void sort_records(std::vector<Record>& records) {
    std::sort(records.begin(), records.end(), [](const Record& a, const Record& b) {
        if (a.score != b.score) return a.score > b.score;
        if (a.name != b.name) return a.name < b.name;
        return a.age < b.age;
    });
}

// ---- 4. std::stable_partition -------------------------------------------------------------------------------------------------
// Moves the adults (age >= 18) to the front and the minors after them, each group keeping its ORIGINAL order. Returns the number of adults.
inline std::size_t partition_adults(std::vector<Record>& records) {
    const auto mid = std::stable_partition(records.begin(), records.end(), [](const Record& r) { return r.age >= 18; });
    return static_cast<std::size_t>(mid - records.begin());
}

// ---- 5. std::accumulate ---------------------------------------------------------------------------------------------------------
// The mean score; 0.0 for no records.
inline double average_score(const std::vector<Record>& records) {
    if (records.empty()) return 0.0;
    const double total = std::accumulate(records.begin(), records.end(), 0.0, [](double sum, const Record& r) { return sum + r.score; });
    return total / static_cast<double>(records.size());
}

// ---- 6. transform, sort, unique, erase ---------------------------------------------------------------------------------------------
// The distinct names, sorted A to Z (case sensitive). std::unique removes only ADJACENT duplicates (so sort first) and, like remove, returns
// the new end: erase the tail.
inline std::vector<std::string> unique_names_sorted(const std::vector<Record>& records) {
    std::vector<std::string> names;
    names.reserve(records.size());
    std::transform(records.begin(), records.end(), std::back_inserter(names), [](const Record& r) { return r.name; });
    std::sort(names.begin(), names.end());
    names.erase(std::unique(names.begin(), names.end()), names.end());
    return names;
}

// ---- 7. std::find_if ---------------------------------------------------------------------------------------------------------------
// The first record with exactly this name, or nothing.
inline std::optional<Record> find_by_name(const std::vector<Record>& records, const std::string& name) {
    const auto it = std::find_if(records.begin(), records.end(), [&](Record r) { return r.name == name; });
    if (it == records.end()) return std::nullopt;
    return *it;
}

// ---- 8. std::lower_bound: binary search in a SORTED range -----------------------------------------------------------------------------
// How many elements x of the sorted vector satisfy lo <= x < hi. Only operator< is used on the elements, and the vector may hold 100000 of
// them: two binary searches, not a scan.
template <class T>
std::size_t count_between(const std::vector<T>& sorted, const T& lo, const T& hi) {
    const auto first = std::lower_bound(sorted.begin(), sorted.end(), lo);  // the first element >= lo
    const auto last = std::lower_bound(sorted.begin(), sorted.end(), hi);   // the first element >= hi
    if (last < first) return 0;                                              // hi < lo: an empty range
    return static_cast<std::size_t>(last - first);
}
