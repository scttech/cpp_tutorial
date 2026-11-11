// A demo of the Bank (read-only). Press Run. ThreadSanitizer is on: it reports a data race even when the totals come out right.
#include <iostream>
#include <thread>
#include <vector>

#include "bank.hpp"

int main() {
    Bank bank(4, 1000);  // four accounts with 1000 each
    std::vector<std::jthread> workers;
    for (int w = 0; w < 4; ++w) {
        workers.emplace_back([&bank, w] {
            for (int i = 0; i < 250; ++i) {
                bank.transfer(static_cast<std::size_t>(w), static_cast<std::size_t>((w + 1) % 4), 10);
                bank.deposit(0, 1);
            }
        });
    }
    workers.clear();  // joins them all
    std::cout << "deposits: 4 x 250 = 1000 more, so the total should be 5000: " << bank.total() << "\n";
    for (std::size_t i = 0; i < bank.size(); ++i) std::cout << "account " << i << ": " << bank.balance(i) << "\n";
}
