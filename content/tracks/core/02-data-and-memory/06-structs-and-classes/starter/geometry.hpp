#pragma once
#include <algorithm>
#include <cmath>

// ---- Point: a plain struct (an aggregate). Keep it that way: no constructors, so Point{3, 4} works.
struct Point {
    double x = 0;
    double y = 0;

    // Straight-line distance to `other`.
    double distance_to(const Point& other) const {
        (void)other;
        return 0;  // TODO
    }

    // A new point moved by (dx, dy). Does not change this one.
    Point translated(double dx, double dy) const {
        (void)dx;
        (void)dy;
        return *this;  // TODO
    }

    bool operator==(const Point&) const = default;
};

// ---- Rectangle: width and height are never negative; the area is cached for speed.
// This starter has a BUG that the compiler warns about: read the warning, and the lesson text on init order.
class Rectangle {
public:
    // A negative width or height becomes 0.
    Rectangle(Point top_left, double width, double height)
        : top_left_(top_left), width_(width), height_(height), area_(width_ * height_) {}

    Point top_left() const { return top_left_; }
    double width() const { return width_; }
    double height() const { return height_; }
    double area() const { return area_; }

    double perimeter() const {
        return 0;  // TODO
    }

    // Is p inside, or exactly on the edge? (top_left is the corner with the smallest x and y.)
    bool contains(const Point& p) const {
        (void)p;
        return false;  // TODO
    }

    // Multiply width and height by `factor` (the top-left corner stays put) and return THIS rectangle,
    // so that calls can be chained: r.scale(2).scale(3). A negative factor is ignored.
    Rectangle& scale(double factor) {
        (void)factor;
        return *this;  // TODO
    }

    // A moved copy. Does not change this rectangle.
    Rectangle translated(double dx, double dy) const {
        (void)dx;
        (void)dy;
        return *this;  // TODO
    }

private:
    Point top_left_;
    double area_;  // cached: always equal to width_ * height_
    double width_;
    double height_;
};

// ---- Interval: the whole numbers from lo() to hi(), both included.
class Interval {
public:
    // The endpoints may come in either order: Interval(9, 3) is the interval 3..9.
    Interval(int a, int b) : lo_(a), hi_(b) {}  // TODO: normalize

    int lo() const { return lo_; }
    int hi() const { return hi_; }

    // hi - lo. (The difference of two ints can be bigger than an int can hold.)
    long long length() const {
        return 0;  // TODO
    }

    bool contains(int value) const {
        (void)value;
        return false;  // TODO
    }

    // Do the two intervals have at least one value in common? 1..3 and 3..5 do.
    bool overlaps(const Interval& other) const {
        (void)other;
        return false;  // TODO
    }

    // The smallest interval that covers both (any gap between them is included).
    Interval merged_with(const Interval& other) const {
        (void)other;
        return *this;  // TODO
    }

    bool operator==(const Interval&) const = default;

private:
    int lo_;
    int hi_;
};

// ---- Reading: three small members. The compiler inserts padding so that each member is aligned.
// Today sizeof(Reading) is 12 although the members add up to 6. Reorder the members (nothing else) to make it 8.
struct Reading {
    char flag;
    int id;
    char kind;
};
