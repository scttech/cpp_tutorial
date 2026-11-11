// The same work with and without an arena (read-only). Press Run: how many trips to the heap does each take?
#include <iostream>
#include <memory_resource>
#include <string>
#include <vector>

#include "arena.hpp"
#include "counting.hpp"

int main() {
    CountingResource heap;  // plays the heap; counts every trip to it

    {
        std::pmr::vector<std::pmr::string> names(&heap);  // the same containers, but every allocation goes straight to the heap
        for (int i = 0; i < 1000; ++i) names.emplace_back("a name that is long enough to need heap memory " + std::to_string(i));
        std::cout << "without an arena: " << heap.allocations << " trips to the heap\n";
    }

    CountingResource heap2;
    {
        Arena arena(1 << 20, &heap2);  // one block of 1 MiB
        std::pmr::vector<std::pmr::string> names(&arena);
        for (int i = 0; i < 1000; ++i) names.emplace_back("a name that is long enough to need heap memory " + std::to_string(i));
        std::cout << "with an arena:    " << heap2.allocations << " trips to the heap, " << arena.used() << " bytes used of " << arena.capacity() << ", "
                  << arena.fallbacks() << " requests did not fit\n";
    }
    std::cout << "all memory given back: " << (heap2.live() == 0 ? "yes" : "NO") << "\n";
}
