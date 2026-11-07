// MUTANT: naive `cin >> name >> age`: breaks on multi-word names and bad ages, and never reports invalid lines.
#include <iostream>
#include <string>

int main() {
    int valid = 0;
    int invalid = 0;
    std::string name;
    int age = 0;
    while (std::cin >> name >> age) {
        std::cout << name << " is " << age << " years old\n";
        ++valid;
    }
    std::cout << "Valid: " << valid << ", Invalid: " << invalid << "\n";
}
