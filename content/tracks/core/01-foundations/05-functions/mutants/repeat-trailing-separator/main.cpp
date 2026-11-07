// MUTANT: appends the separator after every copy, including the last.
#include <algorithm>
#include <cstddef>
#include <iostream>
#include <map>
#include <numeric>
#include <string>
#include <vector>

double mean(const std::vector<double>& values) {
    if (values.empty()) {
        return 0.0;
    }
    return std::accumulate(values.begin(), values.end(), 0.0) / static_cast<double>(values.size());
}

double median(const std::vector<double>& values) {
    if (values.empty()) {
        return 0.0;
    }
    std::vector<double> sorted = values;  // work on a copy: the caller's data stays untouched
    std::sort(sorted.begin(), sorted.end());
    const std::size_t mid = sorted.size() / 2;
    if (sorted.size() % 2 == 1) {
        return sorted[mid];
    }
    return (sorted[mid - 1] + sorted[mid]) / 2.0;
}

int mode(const std::vector<int>& values) {
    if (values.empty()) {
        return 0;
    }
    std::map<int, int> counts;  // ordered by value, so iterating finds the smallest first
    for (int v : values) {
        ++counts[v];
    }
    int best_value = counts.begin()->first;
    int best_count = 0;
    for (const auto& [value, count] : counts) {
        if (count > best_count) {  // strictly greater: ties keep the smaller value
            best_value = value;
            best_count = count;
        }
    }
    return best_value;
}

std::string repeat(const std::string& s, int times = 2, const std::string& sep = "") {
    std::string out;
    for (int i = 0; i < times; ++i) {
        out += s;
        out += sep;
    }
    return out;
}

std::string kind_of(int) {
    return "int";
}
std::string kind_of(double) {
    return "double";
}

int main() {
    std::cout << "repeat(\"ab\") = " << repeat("ab") << "\n";
    std::cout << "repeat(\"ab\", 3) = " << repeat("ab", 3) << "\n";
    std::cout << "repeat(\"ab\", 3, \"-\") = " << repeat("ab", 3, "-") << "\n";

    const std::vector<double> data{3.0, 1.0, 2.0, 10.0};
    std::cout << "mean = " << mean(data) << ", median = " << median(data) << "\n";
    std::cout << "mode = " << mode({4, 1, 4, 2, 2, 4}) << "\n";
    std::cout << "kind_of(3) = " << kind_of(3) << ", kind_of(3.5) = " << kind_of(3.5) << "\n";
}
