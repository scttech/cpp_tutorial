#include <cstddef>
#include <iostream>
#include <map>
#include <string>
#include <string_view>
#include <vector>

// Whitespace is the usual ASCII set: space, \t, \n, \r, \v, \f.

// Split on whitespace. No empty tokens, whatever the spacing looks like.
std::vector<std::string> tokenize(std::string_view text) {
    // TODO
    (void)text;
    return {};
}

// How often does each word occur? A word is a run of ASCII letters (a-z, A-Z); EVERY other byte
// (digits, punctuation, the bytes of non-ASCII characters) separates words. Words are lowercased.
std::map<std::string, int> word_frequencies(std::string_view text) {
    // TODO
    (void)text;
    return {};
}

// `s` without leading and trailing whitespace. Return a VIEW into `s`: no copy, no allocation.
std::string_view trim(std::string_view s) {
    // TODO
    return s;
}

// The first whitespace-delimited word of `s` (skipping leading whitespace), as a view into `s`.
// Empty if there is none.
std::string_view first_word(std::string_view s) {
    // TODO
    (void)s;
    return {};
}

// The strings joined with `sep` between them. Fast tier: build the result with a single allocation.
std::string join(const std::vector<std::string>& parts, std::string_view sep) {
    // TODO
    (void)parts;
    (void)sep;
    return "";
}

// The number of characters in UTF-8 text (code points), not bytes. Assume valid UTF-8.
std::size_t utf8_length(std::string_view s) {
    // TODO
    return s.size();
}

int main() {
    const std::string text = "  The cat and the hat.  THE END!  ";
    for (const auto& w : tokenize(text)) std::cout << "[" << w << "]";
    std::cout << "\n";
    for (const auto& [word, n] : word_frequencies(text)) std::cout << word << "=" << n << " ";
    std::cout << "\n";
    std::cout << "trim: [" << trim(text) << "]\n";

    const std::string_view cafe = "caf\xc3\xa9";  // 5 bytes, 4 characters
    std::cout << cafe << " has " << cafe.size() << " bytes and " << utf8_length(cafe) << " characters\n";
}
