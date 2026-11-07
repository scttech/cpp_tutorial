// MUTANT: < instead of <=, so boundary points are outside.
#include "circle.hpp"

#include <numbers>

double area(const Circle& c) {
    return std::numbers::pi * c.radius * c.radius;
}

double circumference(const Circle& c) {
    return 2.0 * std::numbers::pi * c.radius;
}

bool contains(const Circle& c, double dx, double dy) {
    // compare squared distances: no square root needed
    return dx * dx + dy * dy < c.radius * c.radius;
}
