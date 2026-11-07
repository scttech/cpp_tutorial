#include <iostream>
#include <vector>

// Rotate `values` left by k (negative k rotates right; k may exceed the size), then remove duplicates,
// keeping the first occurrence of each value. Do not modify the input.
std::vector<int> rotate_and_dedupe(const std::vector<int>& values, int k) {
    // TODO
    (void)values;
    (void)k;
    return {};
}

int main() {
    const std::vector<int> input{1, 2, 1, 3, 2, 4};
    for (int x : rotate_and_dedupe(input, 1)) std::cout << x << ' ';
    std::cout << '\n';
}
