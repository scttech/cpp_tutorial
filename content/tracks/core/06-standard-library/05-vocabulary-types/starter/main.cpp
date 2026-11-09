// A demo of your expression tools (read-only). Press Run to see what it prints.
#include <iostream>
#include <vector>

#include "expr.hpp"

int main() {
    // (2 + 3) * (x - 1), and a division by y
    ExprPtr e = bin('*', bin('+', num(2), num(3)), bin('-', var("x"), num(1)));
    ExprPtr d = bin('/', var("x"), var("y"));

    std::cout << "e = " << to_string(*e) << ", " << node_count(*e) << " nodes\n";
    const std::map<std::string, double> vars{{"x", 4.0}, {"y", 0.0}};
    if (auto v = evaluate(*e, vars)) std::cout << "e(x = 4) = " << *v << "\n";
    if (auto v = evaluate(*d, vars)) std::cout << "d = " << *v << "\n";
    else std::cout << "d has no value (division by zero)\n";
    if (!evaluate(*e, {})) std::cout << "e has no value without x\n";

    ExprPtr s = simplify(*e);
    std::cout << "simplified: " << to_string(*s) << "\n";
    for (const std::string& name : variables(*d)) std::cout << "variable " << name << "\n";

    ExprPtr c = clone(*e);
    std::cout << "clone: " << to_string(*c) << "\n";

    const std::vector<double> data{3.5, -1.0, 9.0, 2.0};
    if (auto mm = min_max(data)) {
        auto [lo, hi] = *mm;
        std::cout << "min " << lo << ", max " << hi << "\n";
    }
    const int numbers[] = {7, 8, 9};
    if (auto ht = head_tail(numbers)) {
        auto [head, tail] = *ht;
        std::cout << "head " << head << ", tail has " << tail.size() << " elements\n";
    }
}
