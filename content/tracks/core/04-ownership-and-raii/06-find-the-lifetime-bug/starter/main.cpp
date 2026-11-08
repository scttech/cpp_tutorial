#include <algorithm>
#include <iostream>
#include <memory>
#include <string>
#include <vector>

#include "bugs.hpp"

const std::string& longest_word(const std::vector<std::string>& words) {
    std::string best;
    for (const std::string& w : words) {
        if (w.size() > best.size()) best = w;
    }
    return best;
}

std::string_view label_of(int id) {
    return "item number " + std::to_string(id) + " in the long warehouse";
}

std::vector<int> remove_evens(std::vector<int> v) {
    for (auto it = v.begin(); it != v.end(); ++it) {
        if (*it % 2 == 0) v.erase(it);
    }
    return v;
}

std::function<int()> make_counter(int start) {
    int n = start;
    return [&n]() { return n++; };
}

int use_twice(int id) {
    Widget* w = new Widget{id};
    std::shared_ptr<Widget> a(w);
    std::shared_ptr<Widget> b(w);
    return a->id + b->id;
}

const char* describe(int n) {
    return (std::string("number ") + std::to_string(n) + (n % 2 == 0 ? " is even" : " is odd") +
            " (and this sentence is long enough to need the heap)")
        .c_str();
}

// Each of the six functions has a lifetime bug. Press Run: the sanitizer stops the program at the first one and
// tells you what kind of mistake it is and where. Then use the tests to see which functions are wrong.
int main() {
    std::vector<std::string> words{"pear", "banana", "fig"};
    std::cout << "longest_word: " << longest_word(words) << "\n";
    std::cout << "label_of: " << label_of(7) << "\n";
    std::cout << "remove_evens: " << remove_evens({1, 2, 3, 4, 5, 6}).size() << " values left\n";
    auto next = make_counter(5);
    std::cout << "counter: " << next() << " " << next() << " " << next() << "\n";
    std::cout << "use_twice: " << use_twice(21) << "\n";
    std::cout << "describe: " << describe(3) << "\n";
}
