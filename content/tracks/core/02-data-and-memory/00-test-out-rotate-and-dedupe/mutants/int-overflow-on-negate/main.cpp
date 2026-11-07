// MUTANT: handles negative k by negating it, which overflows for INT_MIN.
#include <algorithm>
#include <unordered_set>
#include <vector>

std::vector<int> rotate_and_dedupe(const std::vector<int>& values, int k) {
    if (values.empty()) return {};
    const int n = static_cast<int>(values.size());
    int shift;
    if (k >= 0) shift = k % n;
    else shift = (n - (-k) % n) % n;
    std::vector<int> rotated = values;
    std::rotate(rotated.begin(), rotated.begin() + shift, rotated.end());
    std::vector<int> result;
    std::unordered_set<int> seen;
    for (int x : rotated) if (seen.insert(x).second) result.push_back(x);
    return result;
}
