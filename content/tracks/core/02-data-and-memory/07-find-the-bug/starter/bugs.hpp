#pragma once
#include <cstddef>
#include <string>
#include <string_view>
#include <vector>

// Six small functions, each with ONE bug. The types and the prototypes live here; the function bodies are in main.cpp.
// A bug can be in either file. Keep every signature as it is.

// A label that shouts: Label("hi").shout() is "hi!".
class Label {
public:
    explicit Label(const std::string& text) : text_(text) {}
    std::string shout() const { return text_ + "!"; }

private:
    const std::string& text_;
};

// A score table row.
struct Entry {
    std::string_view name;
    int score;
};

// 1. Write the squares 0, 1, 4, 9, ... into out[0 .. n-1]. Touch nothing else.
void fill_squares(int* out, std::size_t n);

// 2. A Label whose text is  name + " (label)".
Label make_label(const char* name);

// 3. `heap_value` was allocated with `new`. Return what it holds and free it (take ownership).
int take_and_release(int* heap_value);

// 4. Return the value of v[0] as it is AFTER appending `extra` to v. (v is not empty.)
int first_after_growth(std::vector<int>& v, int extra);

// 5. How many elements of v are greater than zero?
int count_positive(const std::vector<int>& v);

// 6. An Entry whose name is  first + " " + last.
Entry make_entry(const std::string& first, const std::string& last, int score);
