// A demo of your animals (read-only). Press Run to see what it prints.
#include <iostream>
#include <string>
#include <vector>

#include "animals.hpp"

int main() {
    {
        Dog rex("Rex");
        Cat tom("Tom");
        std::cout << rex.describe() << ", says " << rex.speak() << "\n";
        std::cout << tom.describe() << ", says " << tom.speak() << "\n";

        std::cout << "fetch: " << rex.fetch() << ", energy " << rex.energy() << "\n";
        feed(rex, 5);
        std::cout << "after feeding Rex: " << rex.energy() << "\n";

        std::vector<Animal*> zoo{&rex, &tom};
        feed_all(zoo, 2);
        std::cout << "after feed_all: Rex " << rex.energy() << ", Tom " << tom.energy() << "\n";

        const Animal& as_animal = rex;  // a Dog is an Animal...
        std::cout << "through an Animal&: " << as_animal.describe() << "   (the Animal version: describe is not virtual yet)\n";

        rex.learn("sit");
        Animal slice = sliced_copy(rex);  // ... but copying it into an Animal VARIABLE slices the Dog part away
        std::cout << "sliced copy: " << slice.name() << ", " << slice.energy() << " energy (the tricks are gone)\n";

        IntStack stack;
        stack.push(1);
        stack.push(2);
        std::cout << "stack: top " << stack.top() << ", size " << stack.size() << "\n";
    }  // the objects are destroyed here
    std::cout << "the trace of constructors and destructors:\n";
    for (const std::string& line : trace_log()) std::cout << "  " << line << "\n";
}
