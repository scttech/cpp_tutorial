// MUTANT: stores a reference to the local string, which dies on return.
#pragma once
#include <cstddef>
#include <string>
#include <string_view>
#include <vector>

class Label {
public:
    explicit Label(const std::string& text) : text_(text) {}
    std::string shout() const { return text_ + "!"; }

private:
    std::string text_;  // FIX 2: own a copy. A reference member does not keep its target alive.
};

struct Entry {
    const std::string& name;
    int score;
};

void fill_squares(int* out, std::size_t n);
Label make_label(const char* name);
int take_and_release(int* heap_value);
int first_after_growth(std::vector<int>& v, int extra);
int count_positive(const std::vector<int>& v);
Entry make_entry(const std::string& first, const std::string& last, int score);
