// A demo (read-only). Press Run: it counts the operations count_pairs needs for 500, 1000 and 2000 numbers.
#include <iostream>
#include <vector>

#include "count_pairs.hpp"
#include "probe.hpp"

int main() {
    for (int n : {500, 1000, 2000}) {
        std::vector<Num> values;
        for (int i = 0; i < n; ++i) values.push_back(Num((i * 37) % 1000));
        probe::reset();
        const long long pairs = count_pairs(values, Num(1000));
        std::cout << n << " numbers: " << pairs << " pairs, " << probe::ops() << " operations (" << probe::compares << " compares, " << probe::adds << " adds, "
                  << probe::copies << " copies)\n";
    }
    std::cout << "twice the numbers should cost about twice the operations, not four times.\n";
}
