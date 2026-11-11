// MUTANT: the linear search of the starter: correct, and n times slower than it needs to be.
#pragma once

#include <algorithm>
#include <set>
#include <string>
#include <vector>

#include "meter.hpp"

// A report in three stages:   items --dedupe--> unique items --rank--> best first --render--> text
//

inline const char* hot_stage() { return "dedupe"; }

// One item per distinct id: the FIRST item with that id, in the order the ids first appeared.
inline std::vector<Item> dedupe(const std::vector<Item>& items) {
    std::vector<Item> result;
    for (const Item& item : items) {
        bool seen = false;
        for (const Item& kept : result) {
            if (kept == item) {
                seen = true;
                break;
            }
        }
        if (!seen) result.push_back(item);
    }
    return result;
}

// The items with the highest qty first. Items with the same qty keep their relative order.
inline std::vector<Item> rank(std::vector<Item> items) {
    std::stable_sort(items.begin(), items.end(), [](const Item& a, const Item& b) { return a.qty > b.qty; });
    return items;
}

// One line "id:qty" per item.
inline std::string render(const std::vector<Item>& items) {
    std::string out;
    for (const Item& item : items) out += std::to_string(item.id) + ":" + std::to_string(item.qty) + "\n";
    return out;
}

inline std::string report(const std::vector<Item>& items) { return render(rank(dedupe(items))); }
