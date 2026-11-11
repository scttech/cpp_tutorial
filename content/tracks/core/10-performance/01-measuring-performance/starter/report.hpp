#pragma once

#include <algorithm>
#include <string>
#include <vector>

#include "meter.hpp"

// A report in three stages:   items --dedupe--> unique items --rank--> best first --render--> text
//
// STEP 1 (find it):  press Run. main.cpp prints how much work each stage does. Which stage is the HOT SPOT? Change hot_stage() to return
//                    its name: "dedupe", "rank" or "render". Measure, do not guess: the stage that LOOKS the most wasteful is not always the one that costs the most.
// STEP 2 (fix it):   make the hot stage cheap WITHOUT changing the report. The output must stay exactly the same.

inline const char* hot_stage() { return "?"; }

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
