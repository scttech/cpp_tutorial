// MUTANT: keeps the LAST occurrence of each value instead of the first.
#include <algorithm>
#include <unordered_set>
#include <vector>

std::vector<int> rotate_and_dedupe(const std::vector<int>& values, int k) {
    if (values.empty()) return {};
    const long long n = static_cast<long long>(values.size());
    long long shift = static_cast<long long>(k) % n;
    if (shift < 0) shift += n;
    std::vector<int> rotated = values;
    std::rotate(rotated.begin(), rotated.begin() + shift, rotated.end());
    std::vector<int> result;
    std::unordered_set<int> seen;
    for (auto it = rotated.rbegin(); it != rotated.rend(); ++it) if (seen.insert(*it).second) result.push_back(*it);
    std::reverse(result.begin(), result.end());
    return result;
}
