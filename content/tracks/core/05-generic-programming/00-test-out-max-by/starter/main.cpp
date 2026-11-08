// A demo of your max_by (read-only). Press Run to see what it prints.
#include <array>
#include <functional>
#include <iostream>
#include <list>
#include <string>
#include <vector>

#include "max_by.hpp"

struct Person {
    std::string name;
    int age;
};

int main() {
    std::vector<int> numbers{3, 9, 2, 9, 5};
    auto biggest = max_by(numbers, [](int x) { return x; });
    std::cout << "largest: " << *biggest << " at index " << (biggest - numbers.begin()) << " (the first 9)\n";

    std::vector<Person> people{{"Ada", 36}, {"Grace", 45}, {"Alan", 41}};
    std::cout << "oldest: " << max_by(people, &Person::age)->name << "\n";  // a member pointer as the projection

    std::list<std::string> words{"pear", "fig", "banana", "kiwi"};
    std::cout << "longest word: " << *max_by(words, [](const std::string& w) { return w.size(); }) << "\n";

    int fixed[] = {4, 8, 1};
    std::cout << "largest in a C array: " << *max_by(fixed, [](int x) { return x; }) << "\n";
    // With the optional third argument (a comparison of the keys) the same call finds the SMALLEST:
    //   max_by(fixed, [](int x) { return x; }, std::greater<>{})

    const std::vector<int> none;
    std::cout << "empty range gives end(): " << (max_by(none, [](int x) { return x; }) == none.end()) << "\n";
}
