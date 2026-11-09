// A demo of the four ways (read-only). Press Run to see what it prints.
#include <iomanip>
#include <iostream>
#include <vector>

#include "three_ways.hpp"

// A type that has NOTHING to do with the shapes above: no base class, no virtual function. It joins the type-erased collection anyway,
// because it has an `area` function (found by argument-dependent lookup).
struct Square {
    double side;
};
double area(const Square& s) { return s.side * s.side; }

int main() {
    std::cout << std::fixed << std::setprecision(3);
    const std::size_t n = 12;

    auto virtual_shapes = make_virtual(n);
    auto variants = make_variants(n);
    auto erased = make_erased(n);
    std::cout << "total area of " << n << " shapes:\n";
    std::cout << "  virtual functions: " << total_area_virtual(virtual_shapes) << "\n";
    std::cout << "  std::variant:      " << total_area_variant(variants) << "\n";
    std::cout << "  type erasure:      " << total_area_erased(erased) << "\n";

    std::cout << "variant: " << name_of(variants[1]) << " has area " << area_of(variants[1]) << " and perimeter " << perimeter_of(variants[1]) << "\n";

    auto circles = make_static_circles(5);
    std::cout << "CRTP: " << total_area_static(circles) << " for five circles; scaled x2: " << circles[0].scaled_area(2) << "; "
              << circles[0].name() << (circles[1].bigger_than(circles[0]) ? " 1 is bigger than 0" : " 1 is not bigger than 0") << "\n";
    std::cout << "sizeof: virtual circle " << sizeof(VCircle) << ", CRTP circle " << sizeof(SCircle) << " (no vptr), variant " << sizeof(Shape) << "\n";

    erased.emplace_back(Square{10});  // a foreign type, with no base class
    std::vector<AnyShape> copy = erased;  // copies the HELD values: value semantics
    std::cout << "after adding a Square (area 100): " << total_area_erased(copy) << "\n";
}
