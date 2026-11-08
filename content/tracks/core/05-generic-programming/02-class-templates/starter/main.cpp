// A demo of your Stack and Matrix templates (read-only). Press Run to see what it prints.
#include <iostream>
#include <memory>
#include <string>

#include "matrix.hpp"
#include "stack.hpp"

template <class M>
void show(const char* name, const M& m) {
    std::cout << name << " (" << M::rows() << "x" << M::cols() << "):\n";
    for (std::size_t r = 0; r < M::rows(); ++r) {
        for (std::size_t c = 0; c < M::cols(); ++c) std::cout << " " << m(r, c);
        std::cout << "\n";
    }
}

int main() {
    Stack<std::string> words;
    words.push("one");
    words.push("two");
    words.push(std::string("three"));
    std::cout << "size " << words.size() << (words.top() ? ", top " + *words.top() : "") << "\n";
    while (auto w = words.pop()) std::cout << "popped " << *w << "\n";

    Stack<std::unique_ptr<int>> owners;  // a stack of move-only things
    owners.push(std::make_unique<int>(7));
    if (auto p = owners.pop()) std::cout << "unique_ptr popped: " << **p << "\n";

    Stack numbers{10, 20, 30};  // Stack<int>, deduced from the braces
    std::cout << "numbers: size " << numbers.size() << (numbers.top() ? ", top " + std::to_string(*numbers.top()) : "") << "\n";

    Matrix<int, 2, 3> a;
    int n = 1;
    for (std::size_t r = 0; r < 2; ++r)
        for (std::size_t c = 0; c < 3; ++c) a(r, c) = n++;
    show("a", a);
    show("a transposed", a.transposed());
    show("a * its transpose", a * a.transposed());
    show("identity", Matrix<int, 3, 3>::identity());
    // a * a            <- a COMPILE ERROR: you cannot multiply a 2x3 by a 2x3
    // a + a.transposed()  <- also an error: the dimensions differ
}
