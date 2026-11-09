// A demo of inventory.hpp (read-only). Press Run to see it. The tools of this lesson (clang-tidy and cppcheck) look at this file as well.
#include <iostream>
#include <string>
#include <vector>

#include "inventory.hpp"

int main() {
    std::vector<std::string> log;
    std::cout << announce(log, "stock counted") << "\n";

    Inventory inventory;
    inventory.add({"bolt", 120, 0.25});
    inventory.add({"nut", 80, 0.10});
    const Item* bolt = inventory.find("bolt");
    std::cout << "bolt: " << (bolt != nullptr ? bolt->quantity : -1) << ", washer found: " << (inventory.find("washer") != nullptr) << "\n";

    const std::vector<std::string> names = {"bolt", "nut", "washer"};
    std::cout << "total length " << total_length(names) << ", longest " << longest(names) << "\n";
    std::cout << "average of 7 over 2: " << average(7, 2) << "\n";
    std::cout << "unit price of 20 bolts at 1.00: " << unit_price(1.0, 20) << ", of 5: " << unit_price(1.0, 5) << "\n";

    const std::vector<int> counts = bucket_counts({1, 5, 12, 25, 99});
    std::cout << "buckets:";
    for (int c : counts) std::cout << ' ' << c;
    std::cout << "\n";

    Stats stats;
    stats.add(2.0);
    stats.add(4.0);
    std::cout << "mean " << stats.mean() << "\n";
}
