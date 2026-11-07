#include <iostream>
#include <string>
#include <vector>

#include "bugs.hpp"

void fill_squares(int* out, std::size_t n) {
    for (std::size_t i = 0; i <= n; ++i) {
        out[i] = static_cast<int>(i * i);
    }
}

Label make_label(const char* name) {
    return Label(std::string(name) + " (label)");
}

int take_and_release(int* heap_value) {
    delete heap_value;
    return *heap_value;
}

int first_after_growth(std::vector<int>& v, int extra) {
    int* first = &v[0];
    v.push_back(extra);
    return *first;
}

int count_positive(const std::vector<int>& v) {
    int count;
    for (int x : v) {
        if (x > 0) ++count;
    }
    return count;
}

Entry make_entry(const std::string& first, const std::string& last, int score) {
    std::string full = first + " " + last;
    return Entry{full, score};
}

// Each of the six functions has a bug. Run this to see what happens, then use the tests to find out which
// functions are wrong, and read the failure messages carefully: they describe the symptom, not the cause.
int main() {
    std::vector<int> v{1, -2, 3};
    std::cout << "count_positive: " << count_positive(v) << "\n";
    std::cout << "first_after_growth: " << first_after_growth(v, 4) << "\n";
    std::cout << "label: " << make_label("a fairly long label that does not fit in a small string").shout() << "\n";
    const Entry e = make_entry("Ada", "Lovelace-Byron-King", 100);
    std::cout << "entry: " << e.name << " " << e.score << "\n";
}
