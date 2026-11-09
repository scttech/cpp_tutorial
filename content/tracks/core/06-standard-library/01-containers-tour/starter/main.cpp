// A demo of your container functions (read-only). Press Run to see what it prints.
#include <iostream>
#include <list>

#include "containers.hpp"

int main() {
    auto counts = word_counts({"to", "be", "or", "not", "to", "be"});
    for (const auto& [word, n] : counts) std::cout << word << ": " << n << "\n";
    std::cout << "count_of(\"to\") = " << count_of(counts, "to") << ", count_of(\"never\") = " << count_of(counts, "never")
              << ", map size afterwards: " << counts.size() << " (still 4 if count_of only looks)\n";

    for (int v : dedupe({3, 1, 3, 2, 1})) std::cout << v << " ";
    std::cout << "<- dedupe\n";
    for (int v : process_queue({5, 3})) std::cout << v << " ";
    std::cout << "<- process_queue\n";

    std::list<int> items{1, 2, 3, 4};
    move_to_front(items, std::next(items.begin(), 2));
    for (int v : items) std::cout << v << " ";
    std::cout << "<- move_to_front (3 first)\n";

    for (int v : k_smallest(std::vector<int>{9, 4, 7, 1, 8, 2}, 3)) std::cout << v << " ";
    std::cout << "<- k_smallest\n";
}
