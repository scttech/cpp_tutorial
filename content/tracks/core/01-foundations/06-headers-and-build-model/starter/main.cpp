#include <iostream>
#include <vector>

#include "circle.hpp"
#include "shapes.hpp"

int main() {
    const std::vector<Circle> circles{{1.0}, {2.0}, {0.5}};
    std::cout << "total area: " << total_area(circles) << "\n";
    std::cout << "circumference of the first circle: " << circumference(circles[0]) << "\n";
    std::cout << "point (1, 1) is " << (contains(circles[1], 1.0, 1.0) ? "inside" : "outside")
              << " the second circle\n";
}
