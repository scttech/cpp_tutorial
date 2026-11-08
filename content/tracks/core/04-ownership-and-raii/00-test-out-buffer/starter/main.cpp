// A demo of your Buffer (read-only). Press Run to see what it prints.
#include <iostream>
#include <numeric>
#include <utility>
#include <vector>

#include "buffer.hpp"

void show(const char* name, const Buffer& b) {
    std::cout << name << " [" << b.size() << "]:";
    for (int v : b) std::cout << " " << v;
    std::cout << "\n";
}

int main() {
    Buffer a(4, 7);
    a[1] = 1;
    a.at(2) = 2;
    show("a", a);

    Buffer b = a;  // a copy: independent
    b[0] = 99;
    show("a", a);
    show("b", b);

    Buffer c = std::move(b);  // a move: c takes b's block
    show("c", c);
    show("b (moved from)", b);

    c.resize(6);
    show("c resized", c);
    c.resize(2);
    show("c resized", c);

    std::vector<Buffer> many;
    for (int i = 1; i <= 3; ++i) many.push_back(Buffer(i, i));
    int total = 0;
    for (const Buffer& m : many) total += std::accumulate(m.begin(), m.end(), 0);
    std::cout << "total " << total << ", a == a copy: " << (a == Buffer(a)) << "\n";
}
