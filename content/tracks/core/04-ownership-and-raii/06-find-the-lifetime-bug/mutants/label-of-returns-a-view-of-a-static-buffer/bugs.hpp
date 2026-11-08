// MUTANT: a static buffer: the next call overwrites the earlier label.
#pragma once

#include <functional>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

struct Widget {
    int id;
};

// Six functions, one lifetime bug each. The bodies are in main.cpp. A fix may change a return type: change it here
// and in main.cpp together. The tests only use the results.

// 1. The longest word (the FIRST one if several are equally long); "" if there are no words.
//    Returned BY VALUE: the result is a new string that belongs to the caller.
std::string longest_word(const std::vector<std::string>& words);

// 2. A label such as "item number 7 in the long warehouse". A std::string, not a view of a string that is gone.
std::string_view label_of(int id);

// 3. The vector without its even numbers, in the original order.
std::vector<int> remove_evens(std::vector<int> v);

// 4. A counter: the first call returns `start`, the next `start + 1`, and so on. Every counter has its own state.
std::function<int()> make_counter(int start);

// 5. Makes a Widget with this id, uses it through two owners, and returns the sum of the ids it sees: 2 * id.
int use_twice(int id);

// 6. A sentence such as "number 3 is odd (and this sentence is long enough to need the heap)".
std::string describe(int n);
