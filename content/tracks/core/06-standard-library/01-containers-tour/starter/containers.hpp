#pragma once

#include <algorithm>
#include <cstddef>
#include <deque>
#include <list>
#include <map>
#include <queue>
#include <string>
#include <unordered_set>
#include <vector>

// Five small jobs, each with a container that is the RIGHT tool for it. The tests check the results, and the Fast tier checks the COST:
// the wrong container still gives the right answer, only slowly or with a pile of copies.
// Every function is a stub that compiles but is wrong: replace each body.

// ---- 1. std::map: counts, and the keys come out SORTED ---------------------------------------------------------------------
// TODO: word_counts({"b", "a", "b"}) is {a: 1, b: 2}.   Hint: ++counts[w]  (operator[] inserts a zero for a new key)
inline std::map<std::string, int> word_counts(const std::vector<std::string>& words) {
    (void)words;
    return {};
}

// TODO: How many times `word` was counted: 0 for a word that is not in the map. This is a LOOKUP: it must not add the word to the map
// (counts[word] would: operator[] inserts). Use find().
inline int count_of(std::map<std::string, int>& counts, const std::string& word) {
    (void)counts;
    (void)word;
    return -1;
}

// ---- 2. std::unordered_set: "have I seen this?" in O(1) ---------------------------------------------------------------------
// TODO: the values without repeats, in the order of their FIRST occurrence: dedupe({3, 1, 3, 2, 1}) is {3, 1, 2}. Must handle 300000 values:
// searching a vector for each value is O(n) each, O(n^2) in all. insert() on a set tells you whether the value was new (`.second`).
inline std::vector<int> dedupe(const std::vector<int>& values) {
    (void)values;
    return {};
}

// ---- 3. std::deque: a queue (push at the back, pop from the FRONT, both O(1)) -----------------------------------------------
// TODO: Simulates a work queue. Start with the jobs in order. Repeatedly take the job at the FRONT of the queue and record it; if its value is
// greater than 1, put a new job of value (value / 2) at the BACK. Returns the jobs in the order they were processed.
//   process_queue({5, 3}) is {5, 3, 2, 1, 1}   (5 -> adds 2, 3 -> adds 1, 2 -> adds 1, then the 1s)
// Must handle 100000 jobs (about 800000 steps): erasing from the front of a std::vector is O(n) each time.
inline std::vector<int> process_queue(const std::vector<int>& jobs) {
    (void)jobs;
    return {};
}

// ---- 4. std::list: moving an element without touching any other element ----------------------------------------------------
// TODO: Moves the element `it` to the front of the list. It must be O(1) and must not copy or move ANY element: splice relinks the node.
// `it` must stay valid and still refer to the same element (same address).    items.splice(position, source_list, it)
template <class T>
void move_to_front(std::list<T>& items, typename std::list<T>::iterator it) {
    (void)items;
    (void)it;
}

// ---- 5. std::priority_queue: the k smallest of many ---------------------------------------------------------------------------
// TODO: The k smallest values in ascending order (fewer if there are fewer than k; k == 0 gives nothing). Only `operator<` is used on the elements.
// Do not sort everything: keep a max-heap (std::priority_queue<T>) of the k best so far (its top() is the worst of them) and replace the top
// when a smaller value comes. The heap gives the largest first: reverse at the end.
template <class T>
std::vector<T> k_smallest(const std::vector<T>& values, std::size_t k) {
    (void)values;
    (void)k;
    return {};
}
