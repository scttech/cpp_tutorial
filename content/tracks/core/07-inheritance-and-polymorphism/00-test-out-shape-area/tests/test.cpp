#define ZTH_TRACK_ALLOCATIONS
#include <cmath>
#include <cstddef>
#include <limits>
#include <memory>
#include <numbers>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#include "shapes.hpp"
#include "zth_test.hpp"

namespace {

constexpr double kPi = std::numbers::pi;

// A shape written by the TEST: it counts how many of its objects are alive. If deleting a Shape* does not run
// ~Probe, the count stays above zero. (No `override` on purpose: a base with a non-virtual destructor must still compile.)
struct Probe : Shape {
    static inline int alive = 0;
    Probe() { ++alive; }
    Probe(const Probe&) : Shape() { ++alive; }
    ~Probe() { --alive; }
    double area() const { return 1; }
    double perimeter() const { return 4; }
    std::string name() const { return "probe"; }
    std::unique_ptr<Shape> clone() const { return std::make_unique<Probe>(*this); }
};

template <class F>
bool throws_invalid(F f) {
    try {
        f();
    } catch (const std::invalid_argument&) {
        return true;
    } catch (...) {
        return false;
    }
    return false;
}

Shapes sample() {
    Shapes s;
    s.push_back(std::make_unique<Circle>(1));          // pi = 3.14159...
    s.push_back(std::make_unique<Rectangle>(2, 3));    // 6
    s.push_back(std::make_unique<Square>(2));          // 4
    s.push_back(std::make_unique<Triangle>(3, 4, 5));  // 6
    return s;
}

}  // namespace

// ================================================ Works =======================================================
ZTH_TEST("works", "Circle: area, perimeter, name") {
    Circle c(2);
    ZTH_EXPECT_NEAR(c.area(), kPi * 4, 1e-9);
    ZTH_EXPECT_NEAR(c.perimeter(), 4 * kPi, 1e-9);
    ZTH_EXPECT_EQ(c.name(), std::string("circle"));
}
ZTH_TEST("works", "Rectangle: area, perimeter, name") {
    Rectangle r(3, 5);
    ZTH_EXPECT_NEAR(r.area(), 15, 1e-9);
    ZTH_EXPECT_NEAR(r.perimeter(), 16, 1e-9);
    ZTH_EXPECT_EQ(r.name(), std::string("rectangle"));
}
ZTH_TEST("works", "Square: area, perimeter, name") {
    Square s(4);
    ZTH_EXPECT_NEAR(s.area(), 16, 1e-9);
    ZTH_EXPECT_NEAR(s.perimeter(), 16, 1e-9);
    ZTH_EXPECT_EQ(s.name(), std::string("square"));
}
ZTH_TEST("works", "Triangle: area (Heron), perimeter, name") {
    Triangle t(3, 4, 5);
    ZTH_EXPECT_NEAR(t.area(), 6, 1e-9);
    ZTH_EXPECT_NEAR(t.perimeter(), 12, 1e-9);
    ZTH_EXPECT_EQ(t.name(), std::string("triangle"));
    Triangle u(13, 14, 15);
    ZTH_EXPECT_NEAR(u.area(), 84, 1e-9);
}
ZTH_TEST("works", "calls through a Shape reference reach the derived class") {
    Square sq(3);
    const Shape& s = sq;
    ZTH_EXPECT_EQ(s.name(), std::string("square"));
    ZTH_EXPECT_NEAR(s.area(), 9, 1e-9);
    const Rectangle& r = sq;  // even through the Rectangle base
    ZTH_EXPECT_EQ(r.name(), std::string("square"));
}
ZTH_TEST("works", "total_area adds up the areas") {
    const Shapes s = sample();
    ZTH_EXPECT_NEAR(total_area(s), kPi + 6 + 4 + 6, 1e-9);
}
ZTH_TEST("works", "largest finds the shape with the largest area") {
    Shapes s;
    s.push_back(std::make_unique<Square>(1));
    s.push_back(std::make_unique<Circle>(2));
    s.push_back(std::make_unique<Rectangle>(2, 3));
    const Shape* big = largest(s);
    ZTH_EXPECT_TRUE(big != nullptr);
    ZTH_EXPECT_EQ(big->name(), std::string("circle"));
}
ZTH_TEST("works", "clone gives a copy of the real type") {
    const Shapes s = sample();
    for (const auto& shape : s) {
        auto copy = shape->clone();
        ZTH_EXPECT_TRUE(copy != nullptr);
        ZTH_EXPECT_TRUE(copy.get() != shape.get());
        ZTH_EXPECT_EQ(copy->name(), shape->name());
        ZTH_EXPECT_NEAR(copy->area(), shape->area(), 1e-12);
        ZTH_EXPECT_NEAR(copy->perimeter(), shape->perimeter(), 1e-12);
    }
}
ZTH_TEST("works", "clone_all copies every shape") {
    const Shapes s = sample();
    const Shapes copy = clone_all(s);
    ZTH_EXPECT_EQ(copy.size(), s.size());
    for (std::size_t i = 0; i < s.size(); ++i) {
        ZTH_EXPECT_TRUE(copy[i].get() != s[i].get());
        ZTH_EXPECT_EQ(copy[i]->name(), s[i]->name());
    }
}
ZTH_TEST("works", "make_shape builds each kind") {
    ZTH_EXPECT_EQ(make_shape("circle", {1})->name(), std::string("circle"));
    ZTH_EXPECT_EQ(make_shape("rectangle", {2, 3})->name(), std::string("rectangle"));
    ZTH_EXPECT_EQ(make_shape("square", {2})->name(), std::string("square"));
    ZTH_EXPECT_NEAR(make_shape("triangle", {3, 4, 5})->area(), 6, 1e-9);
    ZTH_EXPECT_NEAR(make_shape("rectangle", {2, 3})->area(), 6, 1e-9);
}

// ================================================ Robust ======================================================
ZTH_TEST("robust", "deleting through a Shape pointer runs the derived destructor (the destructor must be virtual)") {
    Probe::alive = 0;
    {
        std::unique_ptr<Shape> p = std::make_unique<Probe>();
        ZTH_EXPECT_EQ(Probe::alive, 1);
    }
    ZTH_EXPECT_EQ(Probe::alive, 0);
}
ZTH_TEST("robust", "a vector of shapes destroys every shape") {
    Probe::alive = 0;
    {
        Shapes s;
        for (int i = 0; i < 5; ++i) s.push_back(std::make_unique<Probe>());
        ZTH_EXPECT_EQ(Probe::alive, 5);
    }
    ZTH_EXPECT_EQ(Probe::alive, 0);
}
ZTH_TEST("robust", "clone_all and clone make real copies that are destroyed too") {
    Probe::alive = 0;
    {
        Shapes s;
        for (int i = 0; i < 3; ++i) s.push_back(std::make_unique<Probe>());
        Shapes copy = clone_all(s);
        ZTH_EXPECT_EQ(Probe::alive, 6);
        ZTH_EXPECT_EQ(copy[1]->name(), std::string("probe"));
    }
    ZTH_EXPECT_EQ(Probe::alive, 0);
}
ZTH_TEST("robust", "the library does not leak or double free") {
    zth::mem::Scope scope;
    {
        Shapes s = sample();
        Shapes copy = clone_all(s);
        total_area(copy);
        largest(copy);
    }
    ZTH_EXPECT_NO_MEMORY_BUGS(scope);
}
ZTH_TEST("robust", "a Square clone is still a Square (Square overrides clone)") {
    Square sq(3);
    auto copy = sq.clone();
    ZTH_EXPECT_EQ(copy->name(), std::string("square"));
    ZTH_EXPECT_TRUE(dynamic_cast<Square*>(copy.get()) != nullptr);
    const Rectangle& r = sq;
    ZTH_EXPECT_EQ(r.clone()->name(), std::string("square"));
}
ZTH_TEST("robust", "a negative or NaN radius is rejected; zero is allowed") {
    ZTH_EXPECT_TRUE(throws_invalid([] { Circle c(-1); }));
    ZTH_EXPECT_TRUE(throws_invalid([] { Circle c(std::numeric_limits<double>::quiet_NaN()); }));
    ZTH_EXPECT_NEAR(Circle(0).area(), 0, 1e-12);
}
ZTH_TEST("robust", "a negative side of a Rectangle or Square is rejected, whichever side") {
    ZTH_EXPECT_TRUE(throws_invalid([] { Rectangle r(-1, 2); }));
    ZTH_EXPECT_TRUE(throws_invalid([] { Rectangle r(2, -1); }));
    ZTH_EXPECT_TRUE(throws_invalid([] { Square s(-3); }));
    ZTH_EXPECT_NEAR(Rectangle(0, 5).area(), 0, 1e-12);
}
ZTH_TEST("robust", "sides that make no triangle are rejected, in any order") {
    ZTH_EXPECT_TRUE(throws_invalid([] { Triangle t(10, 1, 1); }));
    ZTH_EXPECT_TRUE(throws_invalid([] { Triangle t(1, 10, 1); }));
    ZTH_EXPECT_TRUE(throws_invalid([] { Triangle t(1, 1, 10); }));
    ZTH_EXPECT_TRUE(throws_invalid([] { Triangle t(1, 2, 3); }));  // degenerate: a line
    ZTH_EXPECT_TRUE(throws_invalid([] { Triangle t(0, 3, 3); }));
    ZTH_EXPECT_TRUE(throws_invalid([] { Triangle t(-3, 4, 5); }));
    ZTH_EXPECT_NEAR(Triangle(2, 2, 3).area(), std::sqrt(3.5 * 1.5 * 1.5 * 0.5), 1e-9);
}
ZTH_TEST("robust", "make_shape rejects unknown kinds and the wrong number of arguments") {
    ZTH_EXPECT_TRUE(throws_invalid([] { make_shape("hexagon", {1}); }));
    ZTH_EXPECT_TRUE(throws_invalid([] { make_shape("circle", {}); }));
    ZTH_EXPECT_TRUE(throws_invalid([] { make_shape("circle", {1, 2}); }));
    ZTH_EXPECT_TRUE(throws_invalid([] { make_shape("rectangle", {1}); }));
    ZTH_EXPECT_TRUE(throws_invalid([] { make_shape("square", {1, 1}); }));
    ZTH_EXPECT_TRUE(throws_invalid([] { make_shape("triangle", {3, 4}); }));
    ZTH_EXPECT_TRUE(throws_invalid([] { make_shape("circle", {-1}); }));  // the constructor's check still applies
}
ZTH_TEST("robust", "total_area and largest of an empty vector") {
    const Shapes none;
    ZTH_EXPECT_NEAR(total_area(none), 0, 1e-12);
    ZTH_EXPECT_TRUE(largest(none) == nullptr);
}
ZTH_TEST("robust", "null entries are skipped by total_area and largest, and stay null in clone_all") {
    Shapes s;
    s.push_back(nullptr);
    s.push_back(std::make_unique<Square>(2));
    s.push_back(nullptr);
    s.push_back(std::make_unique<Rectangle>(1, 5));
    ZTH_EXPECT_NEAR(total_area(s), 9, 1e-9);
    ZTH_EXPECT_EQ(largest(s)->name(), std::string("rectangle"));
    const Shapes copy = clone_all(s);
    ZTH_EXPECT_EQ(copy.size(), std::size_t{4});
    ZTH_EXPECT_TRUE(copy[0] == nullptr && copy[2] == nullptr);
    ZTH_EXPECT_EQ(copy[1]->name(), std::string("square"));
    Shapes only_null;
    only_null.push_back(nullptr);
    ZTH_EXPECT_TRUE(largest(only_null) == nullptr);
}
ZTH_TEST("robust", "largest: the first of equal areas wins") {
    Shapes s;
    s.push_back(std::make_unique<Rectangle>(2, 2));
    s.push_back(std::make_unique<Square>(2));
    s.push_back(std::make_unique<Rectangle>(1, 4));
    ZTH_EXPECT_EQ(largest(s)->name(), std::string("rectangle"));
    ZTH_EXPECT_TRUE(largest(s) == s[0].get());
}
ZTH_TEST("robust", "largest returns a pointer INTO the vector, not a copy") {
    const Shapes s = sample();
    ZTH_EXPECT_TRUE(largest(s) == s[1].get());  // the rectangle 2x3 (6) comes before the triangle (6)
}

// ================================================ Fast ========================================================
ZTH_TEST("fast", "total_area and largest allocate nothing") {
    const Shapes s = sample();
    zth::mem::Scope scope;
    double sum = 0;
    for (int i = 0; i < 100; ++i) sum += total_area(s) + (largest(s) != nullptr);
    ZTH_EXPECT_TRUE(sum > 0);
    ZTH_EXPECT_ALLOCS_AT_MOST(scope, 0);
}
ZTH_TEST("fast", "clone_all allocates one block per shape plus one for the vector (reserve first)") {
    Shapes s;
    for (int i = 0; i < 100; ++i) s.push_back(std::make_unique<Circle>(i));
    zth::mem::Scope scope;
    const Shapes copy = clone_all(s);
    ZTH_EXPECT_EQ(copy.size(), std::size_t{100});
    ZTH_EXPECT_ALLOCS_AT_MOST(scope, 101);
}
ZTH_TEST("fast", "total_area of many shapes") {
    Shapes s;
    for (int i = 0; i < 20000; ++i) s.push_back(std::make_unique<Square>(1));
    ZTH_EXPECT_NEAR(total_area(s), 20000, 1e-6);
}
