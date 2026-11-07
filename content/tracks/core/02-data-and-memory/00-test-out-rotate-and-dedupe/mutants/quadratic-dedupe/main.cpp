// MUTANT: correct, but searches the output for every element: O(n^2).
#include <algorithm>
#include <vector>

std::vector<int> rotate_and_dedupe(const std::vector<int>& values, int k) {
    if (values.empty()) return {};
    const long long n = static_cast<long long>(values.size());
    long long shift = static_cast<long long>(k) % n;
    if (shift < 0) shift += n;
    std::vector<int> rotated = values;
    std::rotate(rotated.begin(), rotated.begin() + shift, rotated.end());
    std::vector<int> result;
    for (int x : rotated) {
        if (std::find(result.begin(), result.end(), x) == result.end()) result.push_back(x);
    }
    return result;
}
