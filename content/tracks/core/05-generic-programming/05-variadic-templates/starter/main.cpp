// A demo of your variadic templates (read-only). Press Run to see what it prints.
#include <iostream>
#include <string>

#include "variadic.hpp"

struct Greeter {
    std::string greeting;
    int times;
    Greeter(std::string g, int t) : greeting(std::move(g)), times(t) {}
};

int main() {
    std::cout << "count_of(): " << count_of(1) << "\n";
    std::cout << "sum(5) = " << sum(5) << "\n";
    std::cout << "all_true(true) = " << all_true(true) << ", any_true(false) = " << any_true(false) << "\n";
    std::cout << "interpolate: " << interpolate("hello") << "\n";

    // Once the templates are variadic these work (try them):
    //   count_of(1, "a", 2.5) is 3;  sum(1, 2, 3.5) is 6.5;  sum() is 0;  all_true(true, 1, 3 > 2)
    //   interpolate("{} + {} = {}", 1, 2, 3)   and   interpolate("{}: {}", std::string("pi"), 3.14)
    //   auto g = make_owned<Greeter>(std::string("hi"), 3);
    //   for_each_arg([](const auto& x) { std::cout << x << " "; }, 1, "two", 3.0);
}
