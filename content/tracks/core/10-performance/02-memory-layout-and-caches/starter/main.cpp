// How much memory do the loops touch? (read-only) Press Run.
#include <iostream>
#include <list>
#include <numeric>
#include <vector>

#include "cacheline.hpp"
#include "layout.hpp"

int main() {
    std::cout << "sizeof(Record) = " << sizeof(Record) << " bytes (the fields add up to " << sizeof(char) + sizeof(double) + sizeof(char) + sizeof(int) + sizeof(short)
              << ")\n";
    std::cout << "sizeof(ShardCounter) = " << sizeof(ShardCounter) << " bytes\n\n";

    ParticleSystem ps;
    for (int i = 0; i < 4096; ++i) ps.add(float(i), 0, 1, 1, 2, "particle");
    std::cout << "4096 particles, 64-byte cache lines:\n";
    std::cout << "  step()        touches " << lines_for_step(ps) << " lines\n";
    std::cout << "  sum_x()       touches " << lines_for_sum_x(ps) << " lines\n";
    std::cout << "  total_mass()  touches " << lines_for_total_mass(ps) << " lines\n\n";

    std::vector<int> v(4096);
    std::iota(v.begin(), v.end(), 0);
    std::list<int> l(v.begin(), v.end());
    std::vector<const void*> in_vector, in_list;
    for (const int& x : v) in_vector.push_back(&x);
    for (const int& x : l) in_list.push_back(&x);
    std::cout << "4096 ints: a vector spreads over " << lines_touched(in_vector) << " lines, a list over " << lines_touched(in_list)
              << " (and each list step follows a pointer to somewhere else)\n";
}
