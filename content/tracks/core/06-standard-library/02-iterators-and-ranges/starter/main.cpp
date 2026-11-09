// A demo of your iterator tools (read-only). Press Run to see what it prints.
#include <deque>
#include <forward_list>
#include <iostream>
#include <iterator>
#include <list>
#include <sstream>
#include <vector>

#include "iter_tools.hpp"

template <class V>
void show(const char* name, const V& v) {
    std::cout << name << ":";
    for (const auto& x : v) std::cout << " " << x;
    std::cout << "\n";
}

int main() {
    std::cout << "vector:        " << category_name<std::vector<int>::iterator>() << "\n";
    std::cout << "deque:         " << category_name<std::deque<int>::iterator>() << "\n";
    std::cout << "list:          " << category_name<std::list<int>::iterator>() << "\n";
    std::cout << "forward_list:  " << category_name<std::forward_list<int>::iterator>() << "\n";
    std::cout << "istream:       " << category_name<std::istream_iterator<int>>() << "\n";
    std::cout << "back_inserter: " << category_name<std::back_insert_iterator<std::vector<int>>>() << "\n";

    std::list<int> l{1, 2, 3, 4, 5};
    auto it = l.begin();
    advance_by(it, 3);
    std::cout << "advance_by(list, 3) -> " << *it << "\n";

    show("squares_of_evens", squares_of_evens({1, 2, 3, 4, 5, 6}));
    show("middle_section", middle_section({-3, -1, 5, 7, 120, 9}));
    show("split_pieces", split_pieces("a,,b,c", ','));
    show("first_n_matching", first_n_matching([](int x) { return x % 7 == 0; }, 5));
    std::vector<int> v{1, 2, 3};
    duplicate_each(v);
    show("duplicate_each", v);
}
