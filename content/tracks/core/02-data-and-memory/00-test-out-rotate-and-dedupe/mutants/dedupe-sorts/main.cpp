// MUTANT: sort + unique removes duplicates but destroys the order.
#include <algorithm>
#include <vector>

std::vector<int> rotate_and_dedupe(const std::vector<int>& values, int k) {
    if (values.empty()) return {};
    const long long n = static_cast<long long>(values.size());
    long long shift = static_cast<long long>(k) % n;
    if (shift < 0) shift += n;
    std::vector<int> v = values;
    std::rotate(v.begin(), v.begin() + shift, v.end());
    std::sort(v.begin(), v.end());
    v.erase(std::unique(v.begin(), v.end()), v.end());
    return v;
}
