#include <climits>
#include <cstring>
#include <new>

#include "geometry.hpp"
#include "zth_test.hpp"

namespace {
// Can code outside the class touch these members? (Inaccessible members make the requires-expression false.)
template <typename T> constexpr bool has_public_width = requires(T t) { t.width_; };
template <typename T> constexpr bool has_public_height = requires(T t) { t.height_; };
template <typename T> constexpr bool has_public_area = requires(T t) { t.area_; };
template <typename T> constexpr bool has_public_lo = requires(T t) { t.lo_; };
template <typename T> constexpr bool has_public_hi = requires(T t) { t.hi_; };
}  // namespace

// ---- Works ---------------------------------------------------------------------------------------
ZTH_TEST("works", "Point::distance_to: the 3-4-5 triangle") {
    const Point origin{0, 0}, corner{3, 4};
    ZTH_EXPECT_NEAR(origin.distance_to(corner), 5.0, 1e-12);
}
ZTH_TEST("works", "Point::translated returns a moved point") {
    const Point p{1, 2};
    ZTH_EXPECT_TRUE(p.translated(10, 20) == (Point{11, 22}));
}
ZTH_TEST("works", "Rectangle: width, height, area and perimeter") {
    Rectangle r(Point{1, 1}, 4, 3);
    ZTH_EXPECT_NEAR(r.width(), 4, 1e-12);
    ZTH_EXPECT_NEAR(r.height(), 3, 1e-12);
    ZTH_EXPECT_NEAR(r.area(), 12, 1e-12);
    ZTH_EXPECT_NEAR(r.perimeter(), 14, 1e-12);
}
ZTH_TEST("works", "Rectangle::contains: inside and outside") {
    Rectangle r(Point{0, 0}, 10, 5);
    ZTH_EXPECT_TRUE(r.contains(Point{5, 2}));
    ZTH_EXPECT_FALSE(r.contains(Point{11, 2}));
    ZTH_EXPECT_FALSE(r.contains(Point{5, 6}));
}
ZTH_TEST("works", "Rectangle::scale multiplies width, height and area") {
    Rectangle r(Point{0, 0}, 2, 3);
    r.scale(2);
    ZTH_EXPECT_NEAR(r.width(), 4, 1e-12);
    ZTH_EXPECT_NEAR(r.height(), 6, 1e-12);
    ZTH_EXPECT_NEAR(r.area(), 24, 1e-12);
}
ZTH_TEST("works", "Rectangle::translated moves the corner and keeps the size") {
    Rectangle moved = Rectangle(Point{1, 1}, 2, 2).translated(5, -1);
    ZTH_EXPECT_TRUE(moved.top_left() == (Point{6, 0}));
    ZTH_EXPECT_NEAR(moved.area(), 4, 1e-12);
}
ZTH_TEST("works", "Interval: endpoints, length, contains") {
    Interval i(2, 7);
    ZTH_EXPECT_EQ(i.lo(), 2);
    ZTH_EXPECT_EQ(i.hi(), 7);
    ZTH_EXPECT_EQ(i.length(), 5LL);
    ZTH_EXPECT_TRUE(i.contains(4));
    ZTH_EXPECT_FALSE(i.contains(8));
}
ZTH_TEST("works", "Interval normalizes its endpoints: Interval(9, 3) is 3..9") {
    Interval i(9, 3);
    ZTH_EXPECT_EQ(i.lo(), 3);
    ZTH_EXPECT_EQ(i.hi(), 9);
}
ZTH_TEST("works", "Interval::overlaps and merged_with") {
    ZTH_EXPECT_TRUE(Interval(1, 5).overlaps(Interval(4, 9)));
    ZTH_EXPECT_FALSE(Interval(1, 2).overlaps(Interval(5, 6)));
    ZTH_EXPECT_TRUE(Interval(1, 3).merged_with(Interval(2, 9)) == Interval(1, 9));
}
ZTH_TEST("works", "sizeof(Reading) is 8 after reordering its members") {
    ZTH_EXPECT_EQ(sizeof(Reading), std::size_t{8});
}

// ---- Robust --------------------------------------------------------------------------------------
ZTH_TEST("robust", "Point: distance is symmetric, zero to itself, works with negative coordinates") {
    const Point a{-3, -4}, b{0, 0};
    ZTH_EXPECT_NEAR(a.distance_to(b), 5.0, 1e-12);
    ZTH_EXPECT_NEAR(b.distance_to(a), 5.0, 1e-12);
    ZTH_EXPECT_NEAR(a.distance_to(a), 0.0, 1e-12);
}
ZTH_TEST("robust", "Point::translated does not change the original") {
    const Point p{1, 1};
    (void)p.translated(5, 5);
    ZTH_EXPECT_TRUE(p == (Point{1, 1}));
}
ZTH_TEST("robust", "Point is still an aggregate with x and y defaulting to 0") {
    Point p;
    ZTH_EXPECT_NEAR(p.x, 0, 1e-12);
    ZTH_EXPECT_NEAR(p.y, 0, 1e-12);
}
ZTH_TEST("robust", "Rectangle: negative width or height becomes 0, area 0") {
    Rectangle r(Point{0, 0}, -5, 4);
    ZTH_EXPECT_NEAR(r.width(), 0, 1e-12);
    ZTH_EXPECT_NEAR(r.area(), 0, 1e-12);
    Rectangle s(Point{0, 0}, 3, -1);
    ZTH_EXPECT_NEAR(s.height(), 0, 1e-12);
    ZTH_EXPECT_NEAR(s.perimeter(), 6, 1e-12);
}
ZTH_TEST("robust", "Rectangle::contains includes the edges and corners, and nothing just outside") {
    Rectangle r(Point{2, 3}, 4, 5);  // x 2..6, y 3..8
    ZTH_EXPECT_TRUE(r.contains(Point{2, 3}));
    ZTH_EXPECT_TRUE(r.contains(Point{6, 8}));
    ZTH_EXPECT_TRUE(r.contains(Point{6, 3}));
    ZTH_EXPECT_FALSE(r.contains(Point{1.999, 5}));
    ZTH_EXPECT_FALSE(r.contains(Point{4, 8.001}));
    ZTH_EXPECT_FALSE(r.contains(Point{6.001, 5}));
    ZTH_EXPECT_FALSE(r.contains(Point{4, 2.999}));
}
ZTH_TEST("robust", "Rectangle::scale returns the SAME rectangle, so calls chain") {
    Rectangle r(Point{0, 0}, 1, 1);
    r.scale(2).scale(3);
    ZTH_EXPECT_NEAR(r.width(), 6, 1e-12);
    ZTH_EXPECT_NEAR(r.area(), 36, 1e-12);
}
ZTH_TEST("robust", "Rectangle::scale: zero collapses it, a negative factor is ignored, the corner stays put") {
    Rectangle r(Point{4, 4}, 2, 3);
    r.scale(-2);
    ZTH_EXPECT_NEAR(r.area(), 6, 1e-12);
    r.scale(0);
    ZTH_EXPECT_NEAR(r.area(), 0, 1e-12);
    ZTH_EXPECT_TRUE(r.top_left() == (Point{4, 4}));
}
ZTH_TEST("robust", "Rectangle::translated does not change the original") {
    const Rectangle r(Point{1, 1}, 2, 2);
    (void)r.translated(9, 9);
    ZTH_EXPECT_TRUE(r.top_left() == (Point{1, 1}));
}
ZTH_TEST("robust", "Rectangle: members are initialized in declaration order (area must see width and height)") {
    // Construct over memory filled with 0xAB: a member that is read before it is initialized shows up as garbage.
    alignas(Rectangle) unsigned char storage[sizeof(Rectangle)];
    std::memset(storage, 0xAB, sizeof storage);
    Rectangle* r = new (storage) Rectangle(Point{1, 2}, 3, 4);
    ZTH_EXPECT_NEAR(r->area(), 12, 1e-12);
    r->~Rectangle();
}
ZTH_TEST("robust", "Rectangle keeps its data private (encapsulation)") {
    ZTH_EXPECT_FALSE(has_public_width<Rectangle>);
    ZTH_EXPECT_FALSE(has_public_height<Rectangle>);
    ZTH_EXPECT_FALSE(has_public_area<Rectangle>);
}
ZTH_TEST("robust", "Interval keeps its endpoints private") {
    ZTH_EXPECT_FALSE(has_public_lo<Interval>);
    ZTH_EXPECT_FALSE(has_public_hi<Interval>);
}
ZTH_TEST("robust", "Interval: a single point, equal endpoints, negative values") {
    Interval p(4, 4);
    ZTH_EXPECT_EQ(p.length(), 0LL);
    ZTH_EXPECT_TRUE(p.contains(4));
    ZTH_EXPECT_FALSE(p.contains(5));
    Interval n(-3, -9);
    ZTH_EXPECT_EQ(n.lo(), -9);
    ZTH_EXPECT_EQ(n.length(), 6LL);
}
ZTH_TEST("robust", "Interval::length does not overflow int") {
    ZTH_EXPECT_EQ(Interval(INT_MIN, INT_MAX).length(), 4294967295LL);
}
ZTH_TEST("robust", "Interval::contains includes both endpoints") {
    Interval i(1, 3);
    ZTH_EXPECT_TRUE(i.contains(1));
    ZTH_EXPECT_TRUE(i.contains(3));
    ZTH_EXPECT_FALSE(i.contains(0));
}
ZTH_TEST("robust", "Interval::overlaps: touching counts, nested counts, disjoint in both orders does not") {
    ZTH_EXPECT_TRUE(Interval(1, 3).overlaps(Interval(3, 5)));
    ZTH_EXPECT_TRUE(Interval(3, 5).overlaps(Interval(1, 3)));
    ZTH_EXPECT_TRUE(Interval(1, 10).overlaps(Interval(4, 5)));
    ZTH_EXPECT_TRUE(Interval(4, 5).overlaps(Interval(1, 10)));
    ZTH_EXPECT_FALSE(Interval(1, 2).overlaps(Interval(4, 6)));
    ZTH_EXPECT_FALSE(Interval(4, 6).overlaps(Interval(1, 2)));
    ZTH_EXPECT_FALSE(Interval(1, 2).overlaps(Interval(3, 4)));
}
ZTH_TEST("robust", "Interval::merged_with spans a gap and is symmetric") {
    ZTH_EXPECT_TRUE(Interval(1, 2).merged_with(Interval(8, 9)) == Interval(1, 9));
    ZTH_EXPECT_TRUE(Interval(8, 9).merged_with(Interval(1, 2)) == Interval(1, 9));
    ZTH_EXPECT_TRUE(Interval(1, 10).merged_with(Interval(3, 4)) == Interval(1, 10));
}
ZTH_TEST("robust", "Reading still holds its three values") {
    Reading r;
    r.flag = 'f';
    r.id = 123456;
    r.kind = 'k';
    ZTH_EXPECT_EQ(r.flag, 'f');
    ZTH_EXPECT_EQ(r.id, 123456);
    ZTH_EXPECT_EQ(r.kind, 'k');
}
