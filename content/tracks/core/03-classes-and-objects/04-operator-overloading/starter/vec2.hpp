#pragma once

#include <cstddef>
#include <ostream>
#include <stdexcept>

// A 2D vector of ints. It should behave like a number: a + b, a - b, a * 3, 3 * a, -a, a += b, a == b, v[0], ++v,
// v++, if (v), std::cout << v, and dot(a, b). So far there are two flawed attempts: fix them and add the rest.
class Vec2 {
public:
    Vec2() = default;
    Vec2(int x, int y) : x_(x), y_(y) {}

    int x() const { return x_; }
    int y() const { return y_; }

    // TODO: a + b must not change a (and must work on const vectors). Make `+` a non-member that takes the left side
    // by value, and build it from a member `operator+=` that returns `*this` by reference. Same for `-` and `-=`.
    Vec2 operator+(const Vec2& other) {
        x_ += other.x_;
        y_ += other.y_;
        return *this;
    }

    // TODO: `v * 3` and `3 * v` (non-members, built from a member `operator*=(int)`).
    Vec2 operator*(int k) {
        x_ *= k;
        y_ *= k;
        return *this;
    }

    // TODO: unary minus (-v), operator[] (0 is x, 1 is y, anything else throws std::out_of_range; a const and a
    //       non-const overload), `explicit operator bool` (true if not the zero vector), prefix and postfix ++
    //       (add 1 to both parts), `==` (defaulted friend), dot(a, b), and operator<< printing `(3, -4)`.

private:
    int x_ = 0;
    int y_ = 0;
};
