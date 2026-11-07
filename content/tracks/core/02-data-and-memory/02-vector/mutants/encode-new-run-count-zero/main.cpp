// MUTANT: a new run starts with count 0, so every run is one short.
#include <cstddef>
#include <iostream>
#include <vector>

#include "rle.hpp"

std::vector<Run> encode(const std::vector<int>& data) {
    std::vector<Run> runs;
    for (int x : data) {
        if (!runs.empty() && runs.back().value == x) {
            ++runs.back().count;  // extend the current run
        } else {
            runs.push_back({x, 0});
        }
    }
    return runs;
}

std::vector<int> decode(const std::vector<Run>& runs) {
    std::size_t total = 0;
    for (const Run& r : runs) {
        if (r.count > 0) total += static_cast<std::size_t>(r.count);
    }
    std::vector<int> out;
    out.reserve(total);  // one allocation instead of repeated growth
    for (const Run& r : runs) {
        for (int i = 0; i < r.count; ++i) out.push_back(r.value);
    }
    return out;
}

std::vector<long long> squares(int n) {
    std::vector<long long> out;
    if (n <= 0) return out;
    out.reserve(static_cast<std::size_t>(n));
    for (int i = 0; i < n; ++i) {
        out.push_back(static_cast<long long>(i) * i);  // widen BEFORE multiplying: i * i overflows int
    }
    return out;
}

std::vector<int> make_buffer(std::size_t n) {
    std::vector<int> v;
    v.reserve(n);  // reserve changes the capacity; resize would change the size
    return v;
}

void remove_evens(std::vector<int>& v) {
    std::erase_if(v, [](int x) { return x % 2 == 0; });  // the erase-remove idiom, in one call
}

int main() {
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
