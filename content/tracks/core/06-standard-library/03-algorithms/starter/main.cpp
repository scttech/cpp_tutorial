// A demo of your data-cleaning pipeline (read-only). Press Run to see what it prints.
#include <iostream>
#include <vector>

#include "cleaning.hpp"

void show(const char* title, const std::vector<Record>& rs) {
    std::cout << title << ":\n";
    for (const Record& r : rs) std::cout << "  " << r.name << ", " << r.age << ", " << r.score << "\n";
}

int main() {
    std::vector<Record> records = {{"aDA", 36, 91.5}, {"", 20, 50.0}, {"GRACE", 45, 88.0}, {"alan", -3, 70.0}, {"linus", 17, 88.0}, {"Ada", 30, 91.5}};
    remove_invalid(records);
    show("after remove_invalid", records);
    normalize_names(records);
    show("after normalize_names", records);
    sort_records(records);
    show("after sort_records", records);
    const std::size_t adults = partition_adults(records);
    std::cout << adults << " adults first\n";
    std::cout << "average score: " << average_score(records) << "\n";
    for (const auto& n : unique_names_sorted(records)) std::cout << n << " ";
    std::cout << "<- unique names\n";
    if (auto r = find_by_name(records, "Grace")) std::cout << "found Grace, age " << r->age << "\n";
    std::vector<int> sorted{1, 3, 3, 5, 8, 13, 21};
    std::cout << "count_between(3, 13) = " << count_between(sorted, 3, 13) << "\n";
}
