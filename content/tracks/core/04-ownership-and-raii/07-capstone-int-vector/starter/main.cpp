// A demo of your IntVector (read-only). Press Run to see what it prints.
#include <algorithm>
#include <iostream>
#include <utility>

#include "int_vector.hpp"

void show(const char* name, const IntVector& v) {
    std::cout << name << " (size " << v.size() << ", capacity " << v.capacity() << "):";
    for (int x : v) std::cout << " " << x;
    std::cout << "\n";
}

int main() {
    IntVector v;
    for (int i = 1; i <= 9; ++i) {
        v.push_back(i * i);
        std::cout << "after push_back(" << i * i << "): size " << v.size() << ", capacity " << v.capacity() << "\n";
    }
    show("v", v);

    v.insert(0, 100);
    v.erase(5);
    v.push_back(v[0]);  // a reference to an element of v itself
    show("v", v);

    IntVector parens(3, 7);
    IntVector braces{3, 7};
    show("IntVector(3, 7)", parens);
    show("IntVector{3, 7}", braces);

    IntVector copy = v;
    IntVector moved = std::move(v);
    show("copy", copy);
    show("moved", moved);
    show("v (moved from)", v);

    moved.reserve(100);
    moved.shrink_to_fit();
    std::sort(moved.begin(), moved.end());
    show("sorted", moved);
}
