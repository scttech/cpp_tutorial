// A demo of your pretty-printer (read-only). Press Run to see what it prints.
#include <iostream>
#include <map>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "pretty.hpp"

int main() {
    std::cout << to_text(42) << "  " << to_text(true) << "  " << to_text('x') << "  " << to_text(2.5) << "  " << to_text("hi") << "\n";

    std::vector<int> v{1, 2, 3};
    std::cout << to_text(v) << "\n";
    std::cout << to_text(std::make_pair(1, std::string("one"))) << "\n";
    std::cout << to_text(std::optional<int>{}) << " " << to_text(std::optional<int>{5}) << "\n";

    std::map<std::string, std::vector<int>> scores{{"ada", {90, 85}}, {"bob", {70}}};
    std::cout << to_text(scores) << "\n";

    int n = 5;
    int* p = &n;
    int* none = nullptr;
    std::cout << to_text(p) << " " << to_text(none) << "\n";

    std::cout << TypeName<int>::name() << " | " << TypeName<std::vector<int*>>::name() << " | "
              << TypeName<std::pair<int, std::string>>::name() << " | " << TypeName<long>::name() << "\n";
}
