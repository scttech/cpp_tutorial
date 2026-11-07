// MUTANT: computes k % size before checking for an empty vector (division by zero).
#include <algorithm>
#include <unordered_set>
#include <vector>

std::vector<int> rotate_and_dedupe(const std::vector<int>& values, int k) {
    const int n = static_cast<int>(values.size());
    int shift = k % n;
    if (shift < 0) shift += n;
    if (values.empty()) return {};
    std::vector<int> rotated = values;
    std::rotate(rotated.begin(), rotated.begin() + shift, rotated.end());
    std::vector<int> result;
    std::unordered_set<int> seen;
    for (int x : rotated) if (seen.insert(x).second) result.push_back(x);
    return result;
}
