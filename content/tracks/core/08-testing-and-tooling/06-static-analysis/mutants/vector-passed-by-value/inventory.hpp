// MUTANT: the original performance bug.
#pragma once

#include <cstddef>
#include <string>
#include <utility>
#include <vector>

struct Item {
    std::string name;
    int quantity;
    double price;
};

inline std::string announce(std::vector<std::string>& log, std::string message) {
    std::string line = "logged: " + message;
    log.push_back(std::move(message));
    return line;
}

inline std::size_t total_length(std::vector<std::string> names) {
    if (names.empty()) return 0;
    std::size_t total = 0;
    for (const std::string& name : names) total += name.size();
    return total;
}

inline std::string longest(const std::vector<std::string>& names) {
    std::string best;
    for (const std::string& name : names) {
        if (name.size() > best.size()) best = name;
    }
    return best;
}

inline double average(int total, int count) {
    if (count == 0) return 0.0;
    return static_cast<double>(total) / count;
}

inline double unit_price(double base, int quantity) {
    if (quantity >= 10) {
        return base * 0.9;
    }
    return base;
}

inline std::vector<int> bucket_counts(const std::vector<int>& values) {
    int buckets[4];
    for (int i = 0; i < 4; ++i) buckets[i] = 0;
    for (int v : values) {
        int b = v / 10;
        if (b > 3) b = 3;
        if (b < 0) b = 0;
        ++buckets[b];
    }
    return std::vector<int>(buckets, buckets + 4);
}

class Inventory {
public:
    void add(Item item) { items_.push_back(std::move(item)); }

    const Item* find(const std::string& name) const {
        for (const Item& item : items_) {
            if (item.name == name) return &item;
        }
        return nullptr;
    }

    std::size_t size() const { return items_.size(); }

private:
    std::vector<Item> items_;
};

struct Stats {
    int count = 0;
    double sum = 0.0;

    void add(double value) {
        ++count;
        sum += value;
    }
    double mean() const { return count == 0 ? 0.0 : sum / count; }
};
