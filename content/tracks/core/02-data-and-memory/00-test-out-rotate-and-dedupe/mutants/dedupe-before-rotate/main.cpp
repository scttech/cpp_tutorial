// MUTANT: removes duplicates first, then rotates (the two steps do not commute).
#include <algorithm>
#include <unordered_set>
#include <vector>

std::vector<int> rotate_and_dedupe(const std::vector<int>& values, int k) {
    std::vector<int> uniq;
    std::unordered_set<int> seen;
    for (int x : values) if (seen.insert(x).second) uniq.push_back(x);
    if (uniq.empty()) return {};
    const long long n = static_cast<long long>(uniq.size());
    long long shift = static_cast<long long>(k) % n;
    if (shift < 0) shift += n;
    std::rotate(uniq.begin(), uniq.begin() + shift, uniq.end());
    return uniq;
}
