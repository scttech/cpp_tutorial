#ifndef ZTH_CIRCLE_HPP
#define ZTH_CIRCLE_HPP

struct Circle {
    double radius;
};

// Declarations only: the definitions live in circle.cpp.
double area(const Circle& c);
double circumference(const Circle& c);
bool contains(const Circle& c, double dx, double dy);

#endif  // ZTH_CIRCLE_HPP
