// MUTANT: formats without std::fixed, so 8.5 prints as 8.5.
#include <algorithm>
#include <charconv>
#include <climits>
#include <cmath>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

namespace {

struct Item {
    std::string name;
    int quantity;
    double price;
};

std::vector<std::string> split_words(const std::string& line) {
    std::istringstream in(line);
    std::vector<std::string> words;
    std::string word;
    while (in >> word) words.push_back(word);
    return words;
}

// The WHOLE token must be a number: from_chars stops at the first character it cannot use, so check where it stopped.
bool parse_int(const std::string& s, int& out) {
    const char* end = s.data() + s.size();
    const auto [ptr, ec] = std::from_chars(s.data(), end, out);
    return ec == std::errc() && ptr == end;
}

// from_chars happily parses "nan" and "inf", so those are rejected explicitly.
bool parse_price(const std::string& s, double& out) {
    const char* end = s.data() + s.size();
    const auto [ptr, ec] = std::from_chars(s.data(), end, out);
    return ec == std::errc() && ptr == end && std::isfinite(out);
}

std::string money(double value) {
    std::ostringstream os;
    os << std::setprecision(2) << value;
    return os.str();
}

Item* find_item(std::vector<Item>& items, const std::string& name) {
    const auto it = std::find_if(items.begin(), items.end(), [&](const Item& i) { return i.name == name; });
    return it == items.end() ? nullptr : &*it;
}

// Prints one error line and tells the caller to keep running.
bool reject(const std::string& message) {
    std::cout << "error: " << message << "\n";
    return false;
}

// Runs one command. Returns true when the program should stop.
bool run_command(const std::vector<std::string>& w, std::vector<Item>& items) {
    const std::string& cmd = w[0];

    if (cmd == "add") {
        int qty = 0;
        double price = 0;
        if (w.size() != 4 || !parse_int(w[2], qty) || !parse_price(w[3], price)) return reject("usage: add <name> <qty> <price>");
        if (qty <= 0) return reject("quantity must be a positive integer");
        if (price < 0) return reject("price must not be negative");
        if (price == 0) price = 0.0;  // "-0" parses as negative zero, which would print as "-0.00"
        Item* item = find_item(items, w[1]);
        if (item != nullptr) {
            if (item->quantity > INT_MAX - qty) return reject("quantity too large");
            item->quantity += qty;
            item->price = price;
        } else {
            items.push_back({w[1], qty, price});
            item = &items.back();
        }
        std::cout << "added " << qty << " " << w[1] << " (now " << item->quantity << ")\n";
    } else if (cmd == "remove") {
        int qty = 0;
        if (w.size() != 3 || !parse_int(w[2], qty)) return reject("usage: remove <name> <qty>");
        if (qty <= 0) return reject("quantity must be a positive integer");
        Item* item = find_item(items, w[1]);
        if (item == nullptr) return reject("unknown item " + w[1]);
        if (item->quantity < qty) return reject("not enough " + w[1] + " (have " + std::to_string(item->quantity) + ")");
        item->quantity -= qty;
        const int left = item->quantity;
        if (left == 0) items.erase(items.begin() + (item - items.data()));  // erase AFTER reading what we need
        std::cout << "removed " << qty << " " << w[1] << " (now " << left << ")\n";
    } else if (cmd == "price") {
        double price = 0;
        if (w.size() != 3 || !parse_price(w[2], price)) return reject("usage: price <name> <price>");
        if (price < 0) return reject("price must not be negative");
        if (price == 0) price = 0.0;
        Item* item = find_item(items, w[1]);
        if (item == nullptr) return reject("unknown item " + w[1]);
        item->price = price;
        std::cout << "price of " << w[1] << " is now " << money(price) << "\n";
    } else if (cmd == "find") {
        if (w.size() != 2) return reject("usage: find <name>");
        const Item* item = find_item(items, w[1]);
        if (item == nullptr) return reject("unknown item " + w[1]);
        std::cout << item->name << ": " << item->quantity << " @ " << money(item->price) << "\n";
    } else if (cmd == "report") {
        if (w.size() != 1) return reject("usage: report");
        if (items.empty()) {
            std::cout << "inventory is empty\n";
            return false;
        }
        std::vector<Item> sorted = items;  // sort a copy: the stored order is not the report order
        std::sort(sorted.begin(), sorted.end(), [](const Item& a, const Item& b) { return a.name < b.name; });
        double total = 0;
        for (const Item& i : sorted) {
            const double value = i.quantity * i.price;
            total += value;
            std::cout << i.name << " x" << i.quantity << " @ " << money(i.price) << " = " << money(value) << "\n";
        }
        std::cout << "total: " << money(total) << "\n";
    } else if (cmd == "quit") {
        if (w.size() != 1) return reject("usage: quit");
        return true;
    } else {
        std::cout << "error: unknown command " << cmd << "\n";
    }
    return false;
}

}  // namespace

int main() {
    std::vector<Item> items;
    std::string line;
    while (std::getline(std::cin, line)) {
        const std::vector<std::string> words = split_words(line);
        if (words.empty()) continue;  // blank line
        if (run_command(words, items)) break;
    }
    return 0;
}
