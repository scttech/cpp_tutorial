// A demo of your Text class (read-only). Press Run to see what it prints.
#include <iostream>
#include <vector>

#include "text.hpp"

Text shout(Text t) {  // takes a COPY, adds to it, and returns it
    t.append("!");
    return t;
}

int main() {
    Text a("hello");
    Text b = a;  // copy constructor
    b.at(0) = 'J';
    std::cout << "a = " << a.c_str() << ", b = " << b.c_str() << "\n";

    Text c;
    c = b;  // copy assignment
    c.append(" world");
    std::cout << "b = " << b.c_str() << ", c = " << c.c_str() << " (" << c.size() << " chars)\n";

    Text d = shout(a);
    std::cout << "a = " << a.c_str() << ", d = " << d.c_str() << "\n";

    std::vector<Text> many;
    for (int i = 0; i < 4; ++i) many.push_back(a);  // the vector copies its elements when it grows
    std::cout << many.size() << " copies of " << many[3].c_str() << "\n";
}
