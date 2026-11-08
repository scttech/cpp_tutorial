// A demo of your Tree (read-only). Press Run to see what it prints.
#include <iostream>
#include <utility>
#include <vector>

#include "tree.hpp"

void print(const char* label, const Tree& t) {
    std::cout << label << " (" << t.size() << " values, height " << t.height() << "):";
    for (int v : t.to_vector()) std::cout << " " << v;
    std::cout << "\n";
}

int main() {
    Tree t;
    for (int v : {50, 30, 70, 20, 40, 60, 80, 30}) t.insert(v);  // the second 30 is a duplicate
    print("t", t);
    std::cout << "contains 60: " << t.contains(60) << ", contains 65: " << t.contains(65) << "\n";

    t.erase(30);  // a node with two children
    print("after erase 30", t);

    Tree copy = t.clone();  // an independent copy
    copy.insert(99);
    print("t", t);
    print("copy", copy);

    Tree moved = std::move(t);  // a move: t gives its nodes to `moved`
    print("moved", moved);
    print("t after the move", t);
}
