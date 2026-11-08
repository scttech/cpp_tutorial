// MUTANT: it = erase(it) and then ++it: the next element is never looked at.
#include <algorithm>
#include <iostream>
#include <memory>
#include <string>
#include <vector>

#include "bugs.hpp"

// 1. A reference to a local variable is a reference to something that is destroyed at the closing brace. Return by value.
std::string longest_word(const std::vector<std::string>& words) {
    std::string best;
    for (const std::string& w : words) {
        if (w.size() > best.size()) best = w;
    }
    return best;
}

// 2. A string_view does not own its characters; the temporary string is gone at the semicolon. Return the string itself.
std::string label_of(int id) {
    return "item number " + std::to_string(id) + " in the long warehouse";
}

// 3. erase() invalidates the iterator and everything after it. The erase-remove idiom does it in one correct pass.
std::vector<int> remove_evens(std::vector<int> v) {
    for (auto it = v.begin(); it != v.end(); ++it) {
        if (*it % 2 == 0) it = v.erase(it);
    }
    return v;
}

// 4. A lambda that captures a local BY REFERENCE refers to a variable that is gone when the function returns.
//    Capture by value: the counter keeps its own copy (mutable, so that it may change it).
std::function<int()> make_counter(int start) {
    return [n = start]() mutable { return n++; };
}

// 5. Two shared_ptrs built from the same raw pointer have two separate reference counts, so each deletes the Widget.
//    Create it with make_shared and COPY the shared_ptr to share it.
int use_twice(int id) {
    auto a = std::make_shared<Widget>(Widget{id});
    std::shared_ptr<Widget> b = a;
    return a->id + b->id;
}

// 6. c_str() of a temporary string points into memory that is freed at the semicolon. Return the std::string.
std::string describe(int n) {
    return "number " + std::to_string(n) + (n % 2 == 0 ? " is even" : " is odd") +
           " (and this sentence is long enough to need the heap)";
}

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
