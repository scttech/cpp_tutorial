// A demo of parallel_sum (read-only). Press Run. ThreadSanitizer is switched on: a data race in your code stops the program with a report.
#include <iostream>
#include <numeric>
#include <vector>

#include "parallel_sum.hpp"

int main() {
    std::vector<int> values(1000);
    std::iota(values.begin(), values.end(), 1);  // 1, 2, ..., 1000
    for (unsigned threads : {1u, 2u, 4u, 7u}) {
        std::cout << threads << " thread(s): " << parallel_sum(values, threads) << "\n";
    }
    std::cout << "expected: " << std::accumulate(values.begin(), values.end(), 0LL) << "\n";
}
