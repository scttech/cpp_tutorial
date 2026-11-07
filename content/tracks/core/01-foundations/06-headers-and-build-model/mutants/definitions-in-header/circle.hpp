// MUTANT: function bodies in the header without inline: every translation unit gets a definition.
#ifndef ZTH_CIRCLE_HPP
#define ZTH_CIRCLE_HPP
#include <numbers>
struct Circle { double radius; };
double area(const Circle& c) { return std::numbers::pi * c.radius * c.radius; }
double circumference(const Circle& c) { return 2.0 * std::numbers::pi * c.radius; }
bool contains(const Circle& c, double dx, double dy) { return dx * dx + dy * dy <= c.radius * c.radius; }
#endif
