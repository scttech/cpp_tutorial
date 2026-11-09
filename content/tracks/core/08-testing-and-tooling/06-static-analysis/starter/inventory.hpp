#pragma once

#include <cstddef>
#include <string>
#include <utility>
#include <vector>

// Eight small pieces of code that COMPILE WITHOUT A SINGLE WARNING and still contain bugs and bad habits. The compiler cannot see them; two
// static analyzers can: clang-tidy (configured by tidy.yaml) and cppcheck. Press Run Tests: they run after the build, and every finding is listed in
// the Problems tab with its file and line. Fix the CODE, not the tools: the configuration is read-only, and a NOLINT comment is not a fix.

struct Item {
    std::string name;
    int quantity;
    double price;
};

// 1. Writes the message to the log and returns "logged: <message>".
inline std::string announce(std::vector<std::string>& log, std::string message) {
    log.push_back(std::move(message));
    return "logged: " + message;
}

// 2. The sum of the lengths of the names. It must not copy the vector (the tests count allocations).
inline std::size_t total_length(std::vector<std::string> names) {
    if (names.size() == 0) return 0;
    std::size_t total = 0;
    for (const std::string& name : names) total += name.size();
    return total;
}

// 3. The first of the longest names, "" if there are none. It must not copy the names while it looks.
inline std::string longest(const std::vector<std::string>& names) {
    std::string best;
    for (std::string name : names) {
        if (name.size() > best.size()) best = name;
    }
    return best;
}

// 4. total / count as a real number (7 / 2 is 3.5); 0.0 when count is 0.
inline double average(int total, int count) {
    if (count == 0) return 0.0;
    return total / count;
}

// 5. A bulk discount: 10 or more units cost 90% of the base price, fewer cost the full price.
inline double unit_price(double base, int quantity) {
    if (quantity >= 10) {
        return base * 0.9;
    } else {
        return base * 0.9;
    }
}

// 6. How many values fall in [0,10), [10,20), [20,30) and 30 or more. Negative values count in the first bucket.
inline std::vector<int> bucket_counts(const std::vector<int>& values) {
    int buckets[4];
    for (int i = 0; i <= 4; ++i) buckets[i] = 0;
    for (int v : values) {
        int b = v / 10;
        if (b > 3) b = 3;
        if (b < 0) b = 0;
        ++buckets[b];
    }
    return std::vector<int>(buckets, buckets + 4);
}

// 7. A small collection of items: find() returns a pointer to the item with that name, or a null pointer.
class Inventory {
public:
    void add(Item item) { items_.push_back(std::move(item)); }

    const Item* find(const std::string& name) const {
        for (const Item& item : items_) {
            if (item.name == name) return &item;
        }
        return NULL;
    }

    std::size_t size() const { return items_.size(); }

private:
    std::vector<Item> items_;
};

// 8. Running statistics. A new Stats has seen no values: count 0, sum 0, mean 0.
struct Stats {
    int count;
    double sum;

    Stats() {}

    void add(double value) {
        ++count;
        sum += value;
    }
    double mean() const { return count == 0 ? 0.0 : sum / count; }
};
