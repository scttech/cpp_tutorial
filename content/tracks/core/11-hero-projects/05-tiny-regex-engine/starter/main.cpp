// Type a pattern on the first line of the input box and texts on the following lines (a sample if it is empty). Read-only. Press Run.
// For every text: whether it matches as a whole, the first match, and all matches.
#include <iostream>
#include <string>

#include "regex.hpp"

int main() {
    std::string pattern;
    std::string text;
    std::string input, line;
    while (std::getline(std::cin, line)) input += line + "\n";
    if (input.empty()) input = "(ab|c)+d?\nabd\nxxababcdyy\nnothing here\n";
    const std::size_t nl = input.find('\n');
    pattern = input.substr(0, nl);

    const rx::CompileResult c = rx::compile(pattern);
    std::cout << "pattern: " << pattern << "\n";
    if (!c.ok()) {
        std::cout << "error " << rx::name(c.error) << " at " << c.pos << ": " << c.message << "\n";
        return 1;
    }
    std::cout << "NFA states: " << c.regex->nfa().size() << "\n";
    std::size_t at = nl + 1;
    while (at < input.size()) {
        const std::size_t end = input.find('\n', at);
        text = input.substr(at, end - at);
        at = end + 1;
        std::cout << "\ntext: \"" << text << "\"\n";
        std::cout << "  full match: " << (c.regex->full_match(text) ? "yes" : "no") << "\n";
        const auto first = c.regex->search(text);
        if (first) std::cout << "  first match: [" << first->begin << "," << first->end << ") \"" << text.substr(first->begin, first->end - first->begin) << "\"\n";
        else std::cout << "  first match: none\n";
        std::cout << "  all matches:";
        for (const rx::Match& m : c.regex->find_all(text)) std::cout << " [" << m.begin << "," << m.end << ")";
        std::cout << "\n";
    }
}
