// A demo of your shapes (read-only). Press Run to see what it prints.
#include <iomanip>
#include <iostream>
#include <string>
#include <vector>

#include "shapes.hpp"

int main() {
    Shapes shapes;
    shapes.push_back(std::make_unique<Circle>(1.5));
    shapes.push_back(std::make_unique<Rectangle>(3, 4));
    shapes.push_back(std::make_unique<Square>(2));
    shapes.push_back(make_shape("triangle", {3, 4, 5}));

    std::cout << std::fixed << std::setprecision(2);
    for (const auto& s : shapes) std::cout << std::left << std::setw(10) << s->name() << " area " << s->area() << "  perimeter " << s->perimeter() << "\n";
    std::cout << "total area: " << total_area(shapes) << "\n";
    if (const Shape* big = largest(shapes)) std::cout << "largest: " << big->name() << "\n";

    Shapes copy = clone_all(shapes);  // deep copies of the REAL objects: a square stays a square
    std::cout << "the copy of shape 2 is a " << copy[2]->name() << "\n";

    try {
        make_shape("triangle", {1, 2, 10});
    } catch (const std::invalid_argument& e) {
        std::cout << "rejected: " << e.what() << "\n";
    }
}
