// A demo of generator.hpp (read-only). Press Run. Until the Generator is finished, the sequences below print nothing.
#include <iostream>
#include <string>

#include "generator.hpp"

int main() {
    std::cout << "range(3, 8):";
    for (int x : range(3, 8)) std::cout << ' ' << x;
    std::cout << "\n";

    std::cout << "first 12 Fibonacci numbers:";
    for (unsigned long long f : take(fibonacci(), 12)) std::cout << ' ' << f;
    std::cout << "\n";

    std::cout << "words:";
    for (const std::string& w : words("the  quick brown   fox")) std::cout << " [" << w << ']';
    std::cout << "\n";

    // a generator is lazy: nothing runs until you ask for the next value
    Generator<int> g = range(0, 1000000);
    std::cout << "asked for the first two of a million: ";
    for (int i = 0; i < 2 && g.next(); ++i) std::cout << g.value() << ' ';
    std::cout << "\n";
}
