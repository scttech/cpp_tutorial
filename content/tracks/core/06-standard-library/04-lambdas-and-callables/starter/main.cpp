// A demo of your callbacks (read-only). Press Run to see what it prints.
#include <iostream>
#include <string>
#include <vector>

#include "callbacks.hpp"

int twice(int x) { return 2 * x; }
struct AddN {
    int n;
    int operator()(int x) const { return x + n; }
};

int main() {
    EventBus bus;
    std::vector<std::string> log;
    const int a = bus.subscribe("greet", [&log](const std::string& who) { log.push_back("hello " + who); });
    bus.subscribe("greet", [&log](const std::string& who) { log.push_back("welcome " + who); });
    bus.subscribe("bye", [](const std::string& who) { std::cout << "bye " << who << "\n"; });
    std::cout << "published to " << bus.publish("greet", "Ada") << " handlers\n";
    bus.unsubscribe(a);
    std::cout << "after unsubscribe: " << bus.publish("greet", "Grace") << " handler\n";
    for (const std::string& line : log) std::cout << "  " << line << "\n";

    auto next = make_counter(5);
    std::cout << "counter: " << next() << " " << next() << " " << next() << "\n";

    auto multipliers = make_multipliers({2, 10});
    std::cout << "multipliers: " << multipliers[0](3) << " " << multipliers[1](3) << "\n";

    auto plus_one_then_double = compose([](int x) { return x + 1; }, twice);
    std::cout << "compose: " << plus_one_then_double(4) << "\n";

    std::cout << "apply_n(AddN{3}, 4, 0) = " << apply_n(AddN{3}, 4, 0) << ", apply_n(twice, 5, 1) = " << apply_n(twice, 5, 1) << "\n";

    int calls = 0;
    auto once = make_once([&calls] { ++calls; });
    once();
    once();
    once();
    std::cout << "make_once: called the function " << calls << " time(s)\n";
}
