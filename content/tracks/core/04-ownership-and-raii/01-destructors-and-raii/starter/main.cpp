// A demo of your guard and session classes (read-only). Press Run to see what it prints.
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

#include "guard.hpp"

void risky(std::vector<std::string>& log, bool fail) {
    Session outer(log, "outer");
    Session inner(log, "inner");
    if (fail) throw std::runtime_error("something went wrong");
    log.push_back("work done");
}

int main() {
    std::vector<std::string> log;

    risky(log, false);
    try {
        risky(log, true);
    } catch (const std::exception& e) {
        log.push_back(std::string("caught: ") + e.what());
    }
    for (const auto& line : log) std::cout << line << "\n";

    int cleanups = 0;
    {
        ScopeGuard guard([&] { ++cleanups; });
        std::cout << "inside the block, cleanups = " << cleanups << "\n";
    }
    std::cout << "after the block, cleanups = " << cleanups << "\n";
}
