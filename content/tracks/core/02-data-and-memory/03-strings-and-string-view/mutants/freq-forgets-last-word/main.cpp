// MUTANT: does not count the word that ends the text.
#include <cctype>
#include <cstddef>
#include <iostream>
#include <map>
#include <string>
#include <string_view>
#include <vector>

namespace {
// <cctype> functions take an int that must be an unsigned char value (or EOF): cast a plain char first.
bool is_space(char c) { return std::isspace(static_cast<unsigned char>(c)) != 0; }
bool is_letter(char c) { return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z'); }
char lower(char c) { return (c >= 'A' && c <= 'Z') ? static_cast<char>(c - 'A' + 'a') : c; }
}  // namespace

std::vector<std::string> tokenize(std::string_view text) {
    std::vector<std::string> tokens;
    std::size_t i = 0;
    while (i < text.size()) {
        while (i < text.size() && is_space(text[i])) ++i;  // skip whitespace
        const std::size_t start = i;
        while (i < text.size() && !is_space(text[i])) ++i;  // the word
        if (i > start) tokens.emplace_back(text.substr(start, i - start));
    }
    return tokens;
}

std::map<std::string, int> word_frequencies(std::string_view text) {
    std::map<std::string, int> counts;
    std::string word;
    for (char c : text) {
        if (is_letter(c)) {
            word += lower(c);
        } else if (!word.empty()) {
            ++counts[word];
            word.clear();
        }
    }
    return counts;
}

std::string_view trim(std::string_view s) {
    std::size_t begin = 0;
    while (begin < s.size() && is_space(s[begin])) ++begin;
    std::size_t end = s.size();
    while (end > begin && is_space(s[end - 1])) --end;
    return s.substr(begin, end - begin);  // a view of the same characters: nothing is copied
}

std::string_view first_word(std::string_view s) {
    std::size_t begin = 0;
    while (begin < s.size() && is_space(s[begin])) ++begin;
    std::size_t end = begin;
    while (end < s.size() && !is_space(s[end])) ++end;
    return s.substr(begin, end - begin);
}

std::string join(const std::vector<std::string>& parts, std::string_view sep) {
    std::size_t total = 0;
    for (const auto& p : parts) total += p.size();
    if (!parts.empty()) total += sep.size() * (parts.size() - 1);

    std::string out;
    out.reserve(total);  // one allocation for the whole result
    for (std::size_t i = 0; i < parts.size(); ++i) {
        if (i > 0) out += sep;
        out += parts[i];
    }
    return out;
}

std::size_t utf8_length(std::string_view s) {
    std::size_t n = 0;
    for (char c : s) {
        // Continuation bytes look like 10xxxxxx. Every other byte starts a new character.
        if ((static_cast<unsigned char>(c) & 0xC0) != 0x80) ++n;
    }
    return n;
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
