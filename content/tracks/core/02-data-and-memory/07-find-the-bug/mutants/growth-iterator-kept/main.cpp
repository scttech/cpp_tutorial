// MUTANT: keeps a pointer taken before push_back (the original bug).
#include <iostream>
#include <string>
#include <vector>

#include "bugs.hpp"

void fill_squares(int* out, std::size_t n) {
    for (std::size_t i = 0; i < n; ++i) {  // FIX 1: < n, not <= n (valid indexes are 0 .. n-1)
        out[i] = static_cast<int>(i * i);
    }
}

Label make_label(const char* name) {
    return Label(std::string(name) + " (label)");
}

int take_and_release(int* heap_value) {
    const int value = *heap_value;  // FIX 3: read it BEFORE freeing it
    delete heap_value;
    return value;
}

int first_after_growth(std::vector<int>& v, int extra) {
    const int* first = &v[0];
    v.push_back(extra);
    return *first;
}

int count_positive(const std::vector<int>& v) {
    int count = 0;  // FIX 5: initialize
    for (int x : v) {
        if (x > 0) ++count;
    }
    return count;
}

Entry make_entry(const std::string& first, const std::string& last, int score) {
    return Entry{first + " " + last, score};
}

int main() {
    std::vector<int> v{1, -2, 3};
    std::cout << "count_positive: " << count_positive(v) << "\n";
    std::cout << "first_after_growth: " << first_after_growth(v, 4) << "\n";
    std::cout << "label: " << make_label("a fairly long label that does not fit in a small string").shout() << "\n";
    const Entry e = make_entry("Ada", "Lovelace-Byron-King", 100);
    std::cout << "entry: " << e.name << " " << e.score << "\n";
}
