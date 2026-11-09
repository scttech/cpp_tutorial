// MUTANT: the threshold is > 2.
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

// ---- 1. std::map: counts, and the keys come out SORTED ---------------------------------------------------------------------
// word_counts({"b", "a", "b"}) is {a: 1, b: 2}.
inline std::map<std::string, int> word_counts(const std::vector<std::string>& words) {
    std::map<std::string, int> counts;
    for (const std::string& w : words) ++counts[w];  // operator[] inserts a 0 for a new key, then ++ makes it 1
    return counts;
}

// How many times `word` was counted: 0 for a word that is not in the map. This is a LOOKUP: it must not add the word to the map
// (counts[word] would: operator[] inserts).
inline int count_of(std::map<std::string, int>& counts, const std::string& word) {
    const auto it = counts.find(word);
    return it == counts.end() ? 0 : it->second;
}

// ---- 2. std::unordered_set: "have I seen this?" in O(1) ---------------------------------------------------------------------
// The values without repeats, in the order of their FIRST occurrence: dedupe({3, 1, 3, 2, 1}) is {3, 1, 2}. Must handle 300000 values.
inline std::vector<int> dedupe(const std::vector<int>& values) {
    std::unordered_set<int> seen;
    seen.reserve(values.size());
    std::vector<int> result;
    for (int v : values) {
        if (seen.insert(v).second) result.push_back(v);  // insert says whether it was new
    }
    return result;
}

// ---- 3. std::deque: a queue (push at the back, pop from the FRONT, both O(1)) -----------------------------------------------
// Simulates a work queue. Start with the jobs in order. Repeatedly take the job at the FRONT of the queue and record it; if its value is
// greater than 1, put a new job of value (value / 2) at the BACK. Returns the jobs in the order they were processed.
//   process_queue({5, 3}) is {5, 3, 2, 1, 1}   (5 -> adds 2, 3 -> adds 1, 2 -> adds 1, then the 1s)
// Must handle 100000 jobs (about 800000 steps): erasing from the front of a std::vector is O(n) each time.
inline std::vector<int> process_queue(const std::vector<int>& jobs) {
    std::deque<int> queue(jobs.begin(), jobs.end());
    std::vector<int> order;
    while (!queue.empty()) {
        const int job = queue.front();
        queue.pop_front();
        order.push_back(job);
        if (job > 2) queue.push_back(job / 2);
    }
    return order;
}

// ---- 4. std::list: moving an element without touching any other element ----------------------------------------------------
// Moves the element `it` to the front of the list. It must be O(1) and must not copy or move ANY element: splice relinks the node.
// `it` must stay valid and still refer to the same element (same address).
template <class T>
void move_to_front(std::list<T>& items, typename std::list<T>::iterator it) {
    items.splice(items.begin(), items, it);
}

// ---- 5. std::priority_queue: the k smallest of many ---------------------------------------------------------------------------
// The k smallest values in ascending order (fewer if there are fewer than k; k == 0 gives nothing). Only `operator<` is used on the elements.
// Do not sort everything: keep a max-heap of the k best so far (the top is the worst of them) and replace the top when a smaller one comes.
template <class T>
std::vector<T> k_smallest(const std::vector<T>& values, std::size_t k) {
    std::priority_queue<T> worst_first;  // a MAX-heap: top() is the largest
    for (const T& value : values) {
        if (worst_first.size() < k) {
            worst_first.push(value);
        } else if (k > 0 && value < worst_first.top()) {
            worst_first.pop();
            worst_first.push(value);
        }
    }
    std::vector<T> result;
    result.reserve(worst_first.size());
    while (!worst_first.empty()) {  // the largest comes out first...
        result.push_back(worst_first.top());
        worst_first.pop();
    }
    std::reverse(result.begin(), result.end());  // ...so reverse into ascending order
    return result;
}
