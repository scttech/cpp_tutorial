#pragma once
// READ-ONLY. An Item that COUNTS what is done to it. A stopwatch gives a different number on every computer; these counters are the same
// on every run, everywhere, so "how much work did this stage do?" has one answer.
#include <cstddef>
#include <vector>

namespace meter {
inline unsigned long long compares = 0;  // Item == and Item <
inline unsigned long long copies = 0;    // Item copy construction and copy assignment (moves are free)
inline void reset() { compares = copies = 0; }
}  // namespace meter

struct Item {
    int id = 0;   // two items with the same id are duplicates of each other
    int qty = 0;  // the quantity to rank by
    Item() = default;
    Item(int i, int q) : id(i), qty(q) {}
    Item(const Item& o) : id(o.id), qty(o.qty) { ++meter::copies; }
    Item(Item&&) noexcept = default;
    Item& operator=(const Item& o) {
        id = o.id;
        qty = o.qty;
        ++meter::copies;
        return *this;
    }
    Item& operator=(Item&&) noexcept = default;
    friend bool operator==(const Item& a, const Item& b) {
        ++meter::compares;
        return a.id == b.id;
    }
    friend bool operator<(const Item& a, const Item& b) {
        ++meter::compares;
        return a.id < b.id;
    }
};

// n items whose ids repeat a lot (about 2 out of 3 items are a repeat), always the same for the same n and seed.
inline std::vector<Item> make_items(std::size_t n, unsigned seed = 1) {
    std::vector<Item> items;
    items.reserve(n);
    unsigned x = seed * 2654435761u + 12345u;
    const unsigned distinct = static_cast<unsigned>(n / 3 + 1);
    for (std::size_t i = 0; i < n; ++i) {
        x = x * 1664525u + 1013904223u;
        const int id = static_cast<int>((x >> 8) % distinct);
        x = x * 1664525u + 1013904223u;
        items.emplace_back(id, static_cast<int>((x >> 8) % 50));
    }
    return items;
}
