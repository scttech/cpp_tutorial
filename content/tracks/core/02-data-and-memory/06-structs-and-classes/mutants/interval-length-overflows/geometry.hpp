// MUTANT: subtracts in int before widening.
#pragma once
#include <algorithm>
#include <cmath>

struct Point {
    double x = 0;
    double y = 0;

    double distance_to(const Point& other) const { return std::hypot(x - other.x, y - other.y); }

    Point translated(double dx, double dy) const { return Point{x + dx, y + dy}; }

    bool operator==(const Point&) const = default;
};

class Rectangle {
public:
    Rectangle(Point top_left, double width, double height)
        : top_left_(top_left), width_(std::max(width, 0.0)), height_(std::max(height, 0.0)), area_(width_ * height_) {}

    Point top_left() const { return top_left_; }
    double width() const { return width_; }
    double height() const { return height_; }
    double area() const { return area_; }

    double perimeter() const { return 2 * (width_ + height_); }

    bool contains(const Point& p) const {
        return p.x >= top_left_.x && p.x <= top_left_.x + width_ && p.y >= top_left_.y && p.y <= top_left_.y + height_;
    }

    Rectangle& scale(double factor) {
        if (factor >= 0) {
            width_ *= factor;
            height_ *= factor;
            area_ = width_ * height_;  // keep the cache in step
        }
        return *this;  // `this` is a pointer to the object the function was called on
    }

    Rectangle translated(double dx, double dy) const { return Rectangle(top_left_.translated(dx, dy), width_, height_); }

private:
    // Members are initialized in the order they are DECLARED here, whatever order the constructor lists them in.
    // area_ uses width_ and height_, so it must come after them.
    Point top_left_;
    double width_;
    double height_;
    double area_;  // cached: always equal to width_ * height_
};

class Interval {
public:
    Interval(int a, int b) : lo_(std::min(a, b)), hi_(std::max(a, b)) {}

    int lo() const { return lo_; }
    int hi() const { return hi_; }

    long long length() const { return hi_ - lo_; }  // widen first: INT_MAX - INT_MIN overflows int

    bool contains(int value) const { return value >= lo_ && value <= hi_; }

    bool overlaps(const Interval& other) const { return lo_ <= other.hi_ && other.lo_ <= hi_; }

    Interval merged_with(const Interval& other) const { return Interval(std::min(lo_, other.lo_), std::max(hi_, other.hi_)); }

    bool operator==(const Interval&) const = default;

private:
    int lo_;
    int hi_;
};

// Largest member first: no padding is needed between members, only at the end. sizeof is 8.
struct Reading {
    int id;
    char flag;
    char kind;
};
