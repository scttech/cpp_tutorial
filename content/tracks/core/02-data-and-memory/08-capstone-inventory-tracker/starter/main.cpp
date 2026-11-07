#include <iostream>
#include <string>
#include <vector>

// One line of stock.
struct Item {
    std::string name;
    int quantity;
    double price;
};

int main() {
    std::vector<Item> items;  // the whole inventory lives here: a vector owns its elements, so no `new` is needed
    (void)items;

    std::string line;
    while (std::getline(std::cin, line)) {
        // TODO: split the line into words, run the command, print the result
    }
    return 0;
}
