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
// Every function is a stub that compiles but is wrong: replace each body.

struct Record {
    std::string name;
    int age = 0;
    double score = 0.0;
};

// ---- 1. The erase-remove idiom -----------------------------------------------------------------------------------------------
// TODO: remove the invalid records: an empty name, an age below 0 or above 150. The valid ones keep their order.
// std::remove_if does NOT shrink the vector (it moves the keepers to the front and returns the new end): you must erase the tail:
//     v.erase(std::remove_if(v.begin(), v.end(), pred), v.end());
// Must handle 200000 records: erasing one at a time inside a loop is O(n) per erase.
inline void remove_invalid(std::vector<Record>& records) { (void)records; }

// ---- 2. std::transform --------------------------------------------------------------------------------------------------------
// TODO: capitalize each name: first letter upper case, the rest lower case ("aDA" -> "Ada"). ASCII letters only.
// (std::tolower and std::toupper need an unsigned char value: static_cast<unsigned char>(c).)
inline void normalize_names(std::vector<Record>& records) { (void)records; }

// ---- 3. std::sort with a comparator -------------------------------------------------------------------------------------------
// TODO: highest score first; equal scores by name (A to Z); equal names by age (youngest first).
// The comparator must be a STRICT WEAK ORDERING: `<`-like (never true for equal elements), never `<=`.
inline void sort_records(std::vector<Record>& records) { (void)records; }

// ---- 4. std::stable_partition -------------------------------------------------------------------------------------------------
// TODO: move the adults (age >= 18) to the front and the minors after them, each group keeping its ORIGINAL order. Return the number of adults
// (the partition point is the iterator that stable_partition returns).
inline std::size_t partition_adults(std::vector<Record>& records) {
    (void)records;
    return 0;
}

// ---- 5. std::accumulate ---------------------------------------------------------------------------------------------------------
// TODO: the mean score; 0.0 for no records. std::accumulate(first, last, 0.0, [](double sum, const Record& r) { ... })
inline double average_score(const std::vector<Record>& records) {
    (void)records;
    return -1.0;
}

// ---- 6. transform, sort, unique, erase ---------------------------------------------------------------------------------------------
// TODO: the distinct names, sorted A to Z (case sensitive). std::unique removes only ADJACENT duplicates (so sort first) and, like remove, returns
// the new end: erase the tail.
inline std::vector<std::string> unique_names_sorted(const std::vector<Record>& records) {
    (void)records;
    return {};
}

// ---- 7. std::find_if ---------------------------------------------------------------------------------------------------------------
// TODO: the first record with exactly this name, or nothing.
inline std::optional<Record> find_by_name(const std::vector<Record>& records, const std::string& name) {
    (void)records;
    (void)name;
    return std::nullopt;
}

// ---- 8. std::lower_bound: binary search in a SORTED range -----------------------------------------------------------------------------
// TODO: how many elements x of the sorted vector satisfy lo <= x < hi. Only operator< is used on the elements, and the vector may hold 100000 of
// them: two binary searches (std::lower_bound for lo and for hi), not a scan. If hi < lo the answer is 0.
template <class T>
std::size_t count_between(const std::vector<T>& sorted, const T& lo, const T& hi) {
    (void)sorted;
    (void)lo;
    (void)hi;
    return 0;
}
