// A profile of the report (read-only). Press Run and read the table: which stage does the most work?
#include <iomanip>
#include <iostream>

#include "meter.hpp"
#include "report.hpp"

int main() {
    const std::vector<Item> items = make_items(3000);
    std::cout << std::left << std::setw(10) << "stage" << std::right << std::setw(14) << "compares" << std::setw(12) << "copies" << "\n";

    meter::reset();
    const std::vector<Item> unique = dedupe(items);
    std::cout << std::left << std::setw(10) << "dedupe" << std::right << std::setw(14) << meter::compares << std::setw(12) << meter::copies << "\n";

    meter::reset();
    const std::vector<Item> ranked = rank(unique);  // rank takes its argument by value: look at the copies
    std::cout << std::left << std::setw(10) << "rank" << std::right << std::setw(14) << meter::compares << std::setw(12) << meter::copies << "\n";

    meter::reset();
    const std::string text = render(ranked);
    std::cout << std::left << std::setw(10) << "render" << std::right << std::setw(14) << meter::compares << std::setw(12) << meter::copies << "\n";

    std::cout << "\n" << items.size() << " items in, " << unique.size() << " unique, " << text.size() << " characters of report\n";
    std::cout << "your answer: hot_stage() = \"" << hot_stage() << "\"\n";
}
