#include <iostream>

#include "geometry.hpp"

// A small demo of the types you write in geometry.hpp. (This file is read-only.)
int main() {
    const Point a{0, 0};
    const Point b{3, 4};
    std::cout << "distance a to b: " << a.distance_to(b) << "\n";

    Rectangle r(Point{1, 1}, 4, 3);
    std::cout << "area " << r.area() << ", perimeter " << r.perimeter() << "\n";
    r.scale(2).scale(0.5);  // each call returns the SAME rectangle, so the calls chain
    std::cout << "after scaling: " << r.width() << " x " << r.height() << "\n";
    std::cout << "contains (2, 2)? " << (r.contains(Point{2, 2}) ? "yes" : "no") << "\n";

    const Interval i(9, 3);  // the endpoints come in either order
    std::cout << "interval " << i.lo() << ".." << i.hi() << ", length " << i.length() << "\n";
    std::cout << "overlaps 8..12? " << (i.overlaps(Interval(8, 12)) ? "yes" : "no") << "\n";

    std::cout << "sizeof(Reading) = " << sizeof(Reading) << " bytes (its members add up to 6)\n";
}
