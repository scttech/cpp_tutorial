// A demo of your Grid (read-only). Press Run to see what it prints.
#include <iostream>

#include "grid.hpp"

void print(Grid& g) {
    for (std::size_t r = 0; r < g.rows(); ++r) std::cout << g.row_text(r) << "   (row sum " << row_sum(g, r) << ")\n";
    std::cout << "total " << g.total() << ", computed " << g.recomputations() << " time(s)\n";
}

int main() {
    Grid g(2, 3, 1);
    g.at(1, 2) = 10;
    print(g);
    g.total();       // cached: the total is not computed again ...
    g.at(0, 0) = 5;  // ... until the grid may have changed
    print(g);

    // In your finished class, print could take a `const Grid&` (it only reads). Try changing its parameter to
    // `const Grid& g`: with the starter that is a compile error, and with a const-correct Grid it just works.
}
