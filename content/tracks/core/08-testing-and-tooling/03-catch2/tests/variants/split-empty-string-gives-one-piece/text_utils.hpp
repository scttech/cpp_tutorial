#pragma once

#include <cstddef>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

// Four small text helpers. THIS is the code under test: you test it, you do not edit it.
//
//   trim(s)              removes the whitespace (space, \t, \n, \r) at both ends. A string of only whitespace becomes "".
//   split(s, sep)        cuts s at every `sep`; EMPTY pieces are kept: "a,,b" -> {"a", "", "b"} and "a," -> {"a", ""}.
//                        The empty string has no pieces at all: split("", ',') is {} (not {""}).
//   join(parts, sep)     the parts with `sep` BETWEEN them (not after the last one); join({}, sep) is "".
//   parse_int(s)         a decimal int with an optional leading '-'. std::invalid_argument for "", "-", or any character that is not a digit
//                        (also leading or trailing blanks); std::out_of_range if the number does not fit in an int.

inline std::string trim(std::string_view s) {
    constexpr std::string_view ws = " \t\n\r";
    const std::size_t first = s.find_first_not_of(ws);
    if (first == std::string_view::npos) return {};
    const std::size_t last = s.find_last_not_of(ws);
    return std::string(s.substr(first, last - first + 1));
}

inline std::vector<std::string> split(std::string_view s, char sep) {
    std::vector<std::string> parts;
    std::size_t start = 0;
    for (;;) {
        const std::size_t pos = s.find(sep, start);
        if (pos == std::string_view::npos) {
            parts.emplace_back(s.substr(start));
            break;
        }
        parts.emplace_back(s.substr(start, pos - start));
        start = pos + 1;
    }
    return parts;
}

inline std::string join(const std::vector<std::string>& parts, std::string_view sep) {
    std::string out;
    for (std::size_t i = 0; i < parts.size(); ++i) {
        if (i != 0) out += sep;
        out += parts[i];
    }
    return out;
}

inline int parse_int(std::string_view s) {
    if (s.empty()) throw std::invalid_argument("empty text");
    const bool negative = s.front() == '-';
    if (negative) s.remove_prefix(1);
    if (s.empty()) throw std::invalid_argument("no digits");
    const long long limit = negative ? 2147483648LL : 2147483647LL;
    long long value = 0;
    for (const char c : s) {
        if (c < '0' || c > '9') throw std::invalid_argument("not a number");
        value = value * 10 + (c - '0');
        if (value > limit) throw std::out_of_range("number too large");
    }
    return static_cast<int>(negative ? -value : value);
}
