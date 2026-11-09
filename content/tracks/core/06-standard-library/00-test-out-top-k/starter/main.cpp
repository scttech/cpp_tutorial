// A demo of top_k_words (read-only). Press Run to see what it prints.
#include <iostream>

#include "top_k.hpp"

int main() {
    const char* text = "The quick brown fox jumps over the lazy dog. The dog barks; the fox runs. A quick fox!";
    for (const std::string& w : top_k_words(text, 4)) std::cout << w << "\n";
}
