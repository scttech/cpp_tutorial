// A demo of your concepts and constrained templates (read-only). Press Run to see what it prints.
#include <iostream>
#include <list>
#include <string>
#include <vector>

#include "constraints.hpp"

int main() {
    std::vector<int> ints{1, 2, 3, 4};
    std::vector<double> doubles{0.5, 0.25};
    std::cout << "mean of ints: " << mean(ints) << ", of doubles: " << mean(doubles) << "\n";
    std::cout << "larger(1, 2.5) = " << larger(1, 2.5) << ", larger(7, 3) = " << larger(7, 3) << "\n";
    std::cout << "describe: " << describe(1.5) << ", " << describe(std::string("x")) << ", " << describe(true) << "\n";
    // describe(42) is "an integer": try it once the Integer overload exists (it needs Integer to subsume Number)

    Accumulator<int> acc;
    acc.add(10);
    acc.add(20);
    std::cout << "accumulator: total " << acc.total() << ", count " << acc.count() << ", mean " << acc.mean() << "\n";

    std::list<std::string> words{"a", "b", "c"};
    std::cout << "join: " << join(words, "-") << ", " << join(ints, ", ") << "\n";

    // These must NOT compile once the constraints are in (uncomment one to read the error message):
    //   mean(std::vector<std::string>{"a"});       // strings are not Numbers
    //   Accumulator<std::string> bad;               // neither is a std::string
    //   larger(std::string("a"), std::string("b"));
}
