#include <cstddef>
#include <iostream>
#include <vector>

#include "rle.hpp"

// Run-length encoding: {5, 5, 5, 2, 2, 9} becomes {5 x3, 2 x2, 9 x1}.
// Only ADJACENT equal values merge: {1, 2, 1} stays three runs.
std::vector<Run> encode(const std::vector<int>& data) {
    // TODO
    (void)data;
    return {};
}

// The reverse: each run expands into `count` copies. Runs with a count of 0 or less contribute nothing.
// Fast tier: the result should be built with a single heap allocation (you can work out its size first).
std::vector<int> decode(const std::vector<Run>& runs) {
    // TODO
    (void)runs;
    return {};
}

// The squares 0, 1, 4, 9, ... of 0..n-1 (n <= 0 gives an empty vector). The values get big: use long long.
// Fast tier: you know the final size up front, so only one heap allocation should be needed.
std::vector<long long> squares(int n) {
    // TODO
    (void)n;
    return {};
}

// An EMPTY vector (size 0) that can take n elements without reallocating (capacity >= n).
std::vector<int> make_buffer(std::size_t n) {
    // TODO
    (void)n;
    return {};
}

// Remove every even number from `v`, keeping the order of the rest. Do it in place:
// Fast tier: no heap allocation at all. (Careful: erasing inside a plain loop is a classic bug.)
void remove_evens(std::vector<int>& v) {
    // TODO
    (void)v;
}

int main() {
    // How does a vector grow? Watch the capacity jump while the size goes up by one.
    std::vector<int> v;
    std::size_t last_capacity = v.capacity();
    for (int i = 0; i < 20; ++i) {
        v.push_back(i);
        if (v.capacity() != last_capacity) {
            last_capacity = v.capacity();
            std::cout << "size " << v.size() << ": capacity grew to " << last_capacity << "\n";
        }
    }

    for (const Run& r : encode({5, 5, 5, 2, 2, 9})) std::cout << r << "\n";
}
