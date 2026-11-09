// A demo of your time, random and numeric tools (read-only). Press Run to see what it prints.
#include <chrono>
#include <iostream>
#include <random>

#include "sim_tools.hpp"

using namespace std::chrono_literals;

int main() {
    std::cout << "total_time(1min, 30s, 250ms) = " << total_time(1min, 30s, 250ms).count() << " ms\n";
    std::cout << "1500 ms = " << to_fractional_seconds(1500ms) << " s\n";
    std::cout << "pretty: " << pretty_duration(3723004ms) << ", " << pretty_duration(0ms) << ", " << pretty_duration(-1500ms) << "\n";

    Stopwatch<std::chrono::steady_clock> watch;
    watch.start();
    std::cout << "stopwatch: started (elapsed " << watch.elapsed().count() << " ms)\n";

    std::cout << "same seed, same rolls:";
    for (int r : simulate_rolls(42, 12, 6)) std::cout << " " << r;
    std::cout << "\n";

    std::vector<int> deck{1, 2, 3, 4, 5, 6, 7, 8};
    std::mt19937 rng(7);
    shuffle_deterministic(deck, rng);
    std::cout << "shuffled:";
    for (int c : deck) std::cout << " " << c;
    std::cout << "\n";

    std::cout << "prefix_sums:";
    for (long long s : prefix_sums({1, 2, 3, 4})) std::cout << " " << s;
    std::cout << "\n";
    std::cout << "dot_product = " << dot_product({100000, 3}, {100000, 4}) << ", gcd_of_all = " << gcd_of_all({12, 18, 30}) << "\n";

    std::cout << "0.1 + 0.2 == 0.3 ? " << (0.1 + 0.2 == 0.3) << ", nearly_equal ? " << nearly_equal(0.1 + 0.2, 0.3) << "\n";
    std::vector<double> tenths(1000000, 0.1);
    double plain = 0;
    for (double x : tenths) plain += x;
    std::cout.precision(12);
    std::cout << "a million 0.1: plain " << plain << ", sum_accurate " << sum_accurate(tenths) << "\n";
}
