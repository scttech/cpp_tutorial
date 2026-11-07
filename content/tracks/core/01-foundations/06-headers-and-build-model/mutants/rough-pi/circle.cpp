// MUTANT: uses 3.14 for pi.
#include "circle.hpp"

#include <numbers>

double area(const Circle& c) {
    return 3.14 * c.radius * c.radius;
}

double circumference(const Circle& c) {
    return 2.0 * 3.14 * c.radius;
}

bool contains(const Circle& c, double dx, double dy) {
    // compare squared distances: no square root needed
    return dx * dx + dy * dy <= c.radius * c.radius;
}
