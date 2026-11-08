// MUTANT: (3, 0) counts as zero: && instead of ||.
#pragma once

#include <cstddef>
#include <ostream>
#include <stdexcept>

// A 2D vector of ints with the operators you would expect from a number-like type.
class Vec2 {
public:
    Vec2() = default;
    Vec2(int x, int y) : x_(x), y_(y) {}

    int x() const { return x_; }
    int y() const { return y_; }

    // Compound assignment CHANGES the object, so it is a member, and it returns *this by reference (so that
    // `(a += b) += c` and `x = (y += z)` behave like they do for int).
    Vec2& operator+=(const Vec2& other) {
        x_ += other.x_;
        y_ += other.y_;
        return *this;
    }
    Vec2& operator-=(const Vec2& other) {
        x_ -= other.x_;
        y_ -= other.y_;
        return *this;
    }
    Vec2& operator*=(int k) {
        x_ *= k;
        y_ *= k;
        return *this;
    }

    // Unary minus: a new vector, the object itself is not changed (const).
    Vec2 operator-() const { return Vec2(-x_, -y_); }

    // Subscript: two overloads, like at() in the last lesson.
    int& operator[](std::size_t i) {
        if (i > 1) throw std::out_of_range("Vec2: index must be 0 or 1");
        return i == 0 ? x_ : y_;
    }
    const int& operator[](std::size_t i) const {
        if (i > 1) throw std::out_of_range("Vec2: index must be 0 or 1");
        return i == 0 ? x_ : y_;
    }

    // `explicit`, so that `if (v)` works but `int n = v;` and `v + 1` do not.
    explicit operator bool() const { return x_ != 0 && y_ != 0; }

    // Prefix: change, then return the object itself. Postfix (the dummy int parameter): return the OLD value.
    Vec2& operator++() {
        ++x_;
        ++y_;
        return *this;
    }
    Vec2 operator++(int) {
        Vec2 old = *this;
        ++*this;
        return old;
    }

    // Defaulted: compares every member in order. (!= comes for free in C++20.)
    friend bool operator==(const Vec2&, const Vec2&) = default;

private:
    int x_ = 0;
    int y_ = 0;
};

// Binary operators do NOT change their operands, and they are built from the compound ones: take the left side BY VALUE
// (that is the copy you were going to make anyway), modify it, return it. Non-members, so that 2 * v works too.
inline Vec2 operator+(Vec2 a, const Vec2& b) { return a += b; }
inline Vec2 operator-(Vec2 a, const Vec2& b) { return a -= b; }
inline Vec2 operator*(Vec2 v, int k) { return v *= k; }
inline Vec2 operator*(int k, const Vec2& v) { return v * k; }

inline int dot(const Vec2& a, const Vec2& b) { return a.x() * b.x() + a.y() * b.y(); }

// Output: a non-member (the stream is on the left), taking and returning the stream by reference so it can be chained.
inline std::ostream& operator<<(std::ostream& os, const Vec2& v) { return os << '(' << v.x() << ", " << v.y() << ')'; }
