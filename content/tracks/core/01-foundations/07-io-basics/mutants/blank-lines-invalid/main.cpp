// MUTANT: blank lines are reported as invalid instead of ignored.
#include <charconv>
#include <iostream>
#include <string>
#include <string_view>
#include <system_error>

namespace {

constexpr std::string_view kWhitespace = " \t\r\n";

std::string_view trim(std::string_view s) {
    const std::size_t first = s.find_first_not_of(kWhitespace);
    if (first == std::string_view::npos) {
        return {};
    }
    const std::size_t last = s.find_last_not_of(kWhitespace);
    return s.substr(first, last - first + 1);
}

// Parses "<name> <age>"; returns true and fills the outputs only for a valid line.
bool parse_person(std::string_view line, std::string_view& name, int& age) {
    const std::string_view text = trim(line);
    const std::size_t split = text.find_last_of(kWhitespace);
    if (split == std::string_view::npos) {
        return false;  // a single token: no room for both a name and an age
    }
    const std::string_view age_text = text.substr(split + 1);
    const std::string_view candidate = trim(text.substr(0, split));
    if (candidate.empty()) {
        return false;
    }
    int parsed = 0;
    const char* end = age_text.data() + age_text.size();
    const auto [ptr, ec] = std::from_chars(age_text.data(), end, parsed);
    if (ec != std::errc{} || ptr != end || parsed < 0 || parsed > 150) {
        return false;  // not a number, trailing junk like "30x", or out of range
    }
    name = candidate;
    age = parsed;
    return true;
}

}  // namespace

int main() {
    int valid = 0;
    int invalid = 0;
    std::string line;
    while (std::getline(std::cin, line)) {
        std::string_view name;
        int age = 0;
        if (parse_person(line, name, age)) {
            std::cout << name << " is " << age << " years old\n";
            ++valid;
        } else {
            std::cout << "Invalid line: " << line << "\n";
            ++invalid;
        }
    }
    std::cout << "Valid: " << valid << ", Invalid: " << invalid << "\n";
}
