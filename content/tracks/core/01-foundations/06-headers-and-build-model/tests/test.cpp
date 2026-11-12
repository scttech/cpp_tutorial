#include <numbers>
#include <vector>

// The header is deliberately included twice: a missing include guard fails right here, too.
#include "circle.hpp"
#include "circle.hpp"
#include "shapes.hpp"
#include "zth_test.hpp"

// ---- Works ---------------------------------------------------------------------------------------
ZTH_TEST("works", "Circle is a struct with a public radius member") {
    Circle c{2.5};
    ZTH_EXPECT_EQ(c.radius, 2.5);
}
ZTH_TEST("works", "area of a unit circle is pi") {
    ZTH_EXPECT_NEAR(area(Circle{1.0}), std::numbers::pi, 1e-12);
}
ZTH_TEST("works", "area of radius 2 is 4*pi") {
    ZTH_EXPECT_NEAR(area(Circle{2.0}), 4.0 * std::numbers::pi, 1e-12);
}
ZTH_TEST("works", "circumference of radius 2 is 4*pi") {
    ZTH_EXPECT_NEAR(circumference(Circle{2.0}), 4.0 * std::numbers::pi, 1e-12);
}
ZTH_TEST("works", "contains: center and an interior point are inside, a far point is outside") {
    const Circle c{2.0};
    ZTH_EXPECT_TRUE(contains(c, 0.0, 0.0));
    ZTH_EXPECT_TRUE(contains(c, 1.0, 1.0));
    ZTH_EXPECT_FALSE(contains(c, 2.0, 2.0));
}
ZTH_TEST("works", "total_area from shapes.hpp sums the circles (it calls your area)") {
    const std::vector<Circle> circles{{1.0}, {2.0}};
    ZTH_EXPECT_NEAR(total_area(circles), 5.0 * std::numbers::pi, 1e-12);
}
ZTH_TEST("works", "the program output mentions the total area") {
    auto r = ZTH_RUN_USER_MAIN();
    ZTH_EXPECT_CONTAINS(r.out, "total area: ");
    ZTH_EXPECT_CONTAINS(r.out, "inside");
}

// ---- Robust --------------------------------------------------------------------------------------
ZTH_TEST("robust", "contains includes points exactly on the boundary (off-by-one)") {
    const Circle c{5.0};
    ZTH_EXPECT_TRUE(contains(c, 3.0, 4.0));  // distance exactly 5
    ZTH_EXPECT_TRUE(contains(c, 5.0, 0.0));
    ZTH_EXPECT_TRUE(contains(c, 0.0, -5.0));
}
ZTH_TEST("robust", "contains is symmetric in sign") {
    const Circle c{1.0};
    ZTH_EXPECT_TRUE(contains(c, -0.5, -0.5));
    ZTH_EXPECT_FALSE(contains(c, -1.0, 1.0));
}
ZTH_TEST("robust", "a zero-radius circle has zero area and contains only its center") {
    const Circle c{0.0};
    ZTH_EXPECT_EQ(area(c), 0.0);
    ZTH_EXPECT_EQ(circumference(c), 0.0);
    ZTH_EXPECT_TRUE(contains(c, 0.0, 0.0));
    ZTH_EXPECT_FALSE(contains(c, 0.001, 0.0));
}
ZTH_TEST("robust", "total_area of no circles is 0") {
    ZTH_EXPECT_EQ(total_area({}), 0.0);
}
