// MUTANT: k % n is negative for negative k and is used as an offset anyway.
#include <algorithm>
#include <unordered_set>
#include <vector>

std::vector<int> rotate_and_dedupe(const std::vector<int>& values, int k) {
    if (values.empty()) return {};
    const long long n = static_cast<long long>(values.size());
    const long long shift = static_cast<long long>(k) % n;
    std::vector<int> rotated;
    for (long long i = 0; i < n; ++i) rotated.push_back(values[static_cast<std::size_t>((i + shift) % n)]);
    std::vector<int> result;
    std::unordered_set<int> seen;
    for (int x : rotated) if (seen.insert(x).second) result.push_back(x);
    return result;
}
