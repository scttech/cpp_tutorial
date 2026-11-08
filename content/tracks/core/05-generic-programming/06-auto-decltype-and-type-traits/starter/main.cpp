// A demo of your type traits (read-only). Press Run to see what it prints.
#include <iostream>
#include <list>
#include <string>
#include <vector>

#include "traits.hpp"

int& pick(int& a, int&) { return a; }

int main() {
    std::vector<int> v{1, 2, 3};
    const std::list<std::string> words{"a", "b"};
    int array[4] = {1, 2, 3, 4};

    std::cout << std::boolalpha;
    std::cout << "is_container_v<vector<int>>: " << is_container_v<std::vector<int>> << ", <int>: " << is_container_v<int> << "\n";
    std::cout << "count_items: " << count_items(v) << " " << count_items(words) << " " << count_items(array) << " " << count_items(3.5) << "\n";
    std::cout << "first_value: " << first_value(v) << ", first_ref: " << first_ref(v) << "\n";

    // Once the traits work, try these (static_assert is checked by the compiler, not at run time):
    //   static_assert(std::is_same_v<plain_t<const int&>, int>);
    //   static_assert(std::is_same_v<element_type_t<std::vector<double>&>, double>);
    //   static_assert(std::is_same_v<call_result_t<decltype(pick), int&, int&>, int&>);
    //   int a = 1, b = 2;
    //   call_through(pick, a, b) = 99;   // a is 99 now: call_through returned the int& that pick returned
    //   std::cout << a << "\n";
    //   int dst[4];
    //   copy_all(array, dst, 4);
}
