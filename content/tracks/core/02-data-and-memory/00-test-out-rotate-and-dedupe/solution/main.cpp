#include <algorithm>
#include <iostream>
#include <unordered_set>
#include <vector>

std::vector<int> rotate_and_dedupe(const std::vector<int>& values, int k) {
    if (values.empty()) return {};

    // long long: INT_MIN cannot be negated or reduced safely in int, and % of a negative number is negative.
    const long long n = static_cast<long long>(values.size());
    long long shift = static_cast<long long>(k) % n;
    if (shift < 0) shift += n;

    std::vector<int> rotated = values;  // a copy: the input is const
    std::rotate(rotated.begin(), rotated.begin() + shift, rotated.end());

    std::vector<int> result;
    result.reserve(rotated.size());
    std::unordered_set<int> seen;
    seen.reserve(rotated.size());
    for (int x : rotated) {
        if (seen.insert(x).second) result.push_back(x);  // insert reports whether x was new
    }
    return result;
}

int main() {
    const std::vector<int> input{1, 2, 1, 3, 2, 4};
    for (int x : rotate_and_dedupe(input, 1)) std::cout << x << ' ';
    std::cout << '\n';
}
