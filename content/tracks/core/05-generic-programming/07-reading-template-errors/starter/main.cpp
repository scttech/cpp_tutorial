#include <iostream>
#include <string>
#include <vector>

#include "generic_bugs.hpp"

int stack_demo() {
    Stack s;
    s.push(3);
    s.push(4);
    const int top = s.pop();
    return top * 10 + s.pop();
}

std::size_t buffer_demo() {
    std::size_t n = 4;
    Buffer<n> b;
    b.data[0] = 1;
    return Buffer<n>::capacity();
}

int main() {
    std::cout << "largest(3, 4.5) = " << largest(3, 4.5) << "\n";
    std::cout << "first_of = " << first_of(std::vector<int>{5, 6}) << "\n";
    std::cout << "twice(2.5) = " << twice(2.5) << ", twice(21) = " << twice(21) << "\n";
    const auto sorted = sorted_versions({{1, 10}, {1, 2}, {0, 9}});
    std::cout << "sorted versions:";
    for (const Version& v : sorted) std::cout << " " << v.major << "." << v.minor;
    std::cout << "\n";
    std::cout << "stack_demo = " << stack_demo() << ", buffer_demo = " << buffer_demo() << "\n";
}
