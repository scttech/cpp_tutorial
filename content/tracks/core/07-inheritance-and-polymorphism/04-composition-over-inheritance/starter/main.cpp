// A demo of your parts (read-only). Press Run to see what it prints.
#include <iostream>
#include <memory>
#include <vector>

#include "composition.hpp"

int main() {
    // Part A: a bag that counts what it was given
    CountingBag bag;
    bag.add(10);
    bag.add_all({1, 2, 3});
    std::cout << "bag: " << bag.size() << " values, sum " << bag.sum() << ", added() says " << bag.added() << "  (should be 4)\n";

    // Part B: characters built from parts
    Character knight = make_knight("Arthur");
    Character dragon = make_dragon("Smaug");
    std::cout << knight.describe() << ": 5 ticks cover " << knight.distance(5) << ", a level-3 strike does " << knight.strike(3) << "\n";
    std::cout << dragon.describe() << ": 5 ticks cover " << dragon.distance(5) << "\n";

    knight.set_movement(std::make_unique<Fly>());  // the parts can be swapped at run time: a flying knight needs no new class
    std::cout << "after set_movement: " << knight.describe() << ", 5 ticks cover " << knight.distance(5) << "\n";

    dragon.take_hit(120);
    std::cout << "the dragon takes 120 damage: alive? " << dragon.alive() << ", strike " << dragon.strike(1) << "\n";

    Character archer("Robin", 20, std::make_unique<Walk>(), std::make_unique<Bow>());
    std::cout << archer.describe() << ": a level-2 shot does " << archer.strike(2) << "\n";

    // Part C: the diamond
    Copier copier(7);
    Scanner& as_scanner = copier;
    Printer& as_printer = copier;
    as_scanner.id = 9;
    std::cout << "Devices built: " << Device::constructed << " (should be 1)\n";
    std::cout << "printer sees id " << as_printer.id << " after the scanner changed it (should be 9): " << copier.copy() << "\n";
}
