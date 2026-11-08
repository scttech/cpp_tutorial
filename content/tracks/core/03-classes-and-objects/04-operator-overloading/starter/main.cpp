// A demo of your Vec2 (read-only). Press Run to see what it prints.
#include <iostream>

#include "vec2.hpp"

int main() {
    Vec2 a(1, 2);
    Vec2 b(10, 20);
    Vec2 c = a + b;
    Vec2 d = c * 2;
    std::cout << "a = (" << a.x() << ", " << a.y() << ")\n";  // should still be (1, 2) after a + b
    std::cout << "c = a + b = (" << c.x() << ", " << c.y() << ")\n";
    std::cout << "d = c * 2 = (" << d.x() << ", " << d.y() << ")\n";

    // Once your operators exist, these work (try them):
    //   std::cout << "a = " << a << ", -a = " << -a << ", 3 * a = " << 3 * a << "\n";
    //   std::cout << "dot = " << dot(a, b) << ", a == b: " << (a == b) << "\n";
    //   a += b; ++a; a[0] = 100;
    //   if (a) std::cout << "a is not the zero vector\n";
}
