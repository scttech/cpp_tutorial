// MUTANT: case-insensitive level.
#pragma once

#include <charconv>
#include <cmath>
#include <cstddef>
#include <format>
#include <optional>
#include <string>
#include <string_view>
#include <system_error>
#include <vector>

// Text without iostreams and without regular expressions: std::from_chars / std::to_chars (fast, no locale, no allocation, no exceptions),
// std::format (type-safe formatting), and std::string_view (slices of the text, no copies).

// ---- 1. std::from_chars: text to a number ------------------------------------------------------------------------------------------------------
// The WHOLE text must be a decimal integer: an optional '-', then digits. Nothing else: no '+', no spaces, no trailing characters, no overflow.
//   parse_int("42") is 42, parse_int("-7") is -7, parse_int("007") is 7;  parse_int(""), parse_int("12abc"), parse_int(" 5"), parse_int("+5"),
//   parse_int("99999999999") (too big for an int) are all nothing.
// from_chars reports problems through an error code, not an exception, and tells you where it stopped: you must check BOTH.
inline std::optional<int> parse_int(std::string_view text) {
    int value = 0;
    const char* const end = text.data() + text.size();
    const auto [stop, ec] = std::from_chars(text.data(), end, value);
    if (ec != std::errc{} || stop != end) return std::nullopt;
    return value;
}

// The same for a floating-point number ("3.5", "-0.25", "1e3", ".5"). from_chars ALSO accepts "inf" and "nan": those are not numbers for us
// (nothing), and neither is a text with anything after the number.
inline std::optional<double> parse_double(std::string_view text) {
    double value = 0.0;
    const char* const end = text.data() + text.size();
    const auto [stop, ec] = std::from_chars(text.data(), end, value);
    if (ec != std::errc{} || stop != end || !std::isfinite(value)) return std::nullopt;
    return value;
}

// ---- 2. std::to_chars: a number to text -------------------------------------------------------------------------------------------------------------
// The value in lower-case hexadecimal, with no prefix: to_hex(255) is "ff", to_hex(0) is "0", to_hex(4096) is "1000".
inline std::string to_hex(unsigned value) {
    char buffer[16];  // 8 hex digits are enough for a 32-bit value
    const auto result = std::to_chars(buffer, buffer + sizeof buffer, value, 16);
    return std::string(buffer, result.ptr);
}

// ---- 3. std::format: type-safe formatting -------------------------------------------------------------------------------------------------------------
// One table row:  the name LEFT-aligned in 10 columns, '|', the count RIGHT-aligned in 5 columns, '|', the ratio with 2 decimals RIGHT-aligned in 8.
//   format_row("ab", 42, 3.14159) is "ab        |   42|    3.14".   (A name longer than 10 characters is not cut.)
inline std::string format_row(std::string_view name, int count, double ratio) {
    return std::format("{:<10}|{:>5}|{:>8.2f}", name, count, ratio);
}

// ---- 4. std::string_view: slicing text without copying ---------------------------------------------------------------------------------------------------
// The text without leading and trailing ASCII whitespace (space, tab, \r, \n, \v, \f): a VIEW into the original, no copy.
inline std::string_view trim(std::string_view text) {
    constexpr std::string_view whitespace = " \t\r\n\v\f";
    const std::size_t first = text.find_first_not_of(whitespace);
    if (first == std::string_view::npos) return text.substr(0, 0);
    const std::size_t last = text.find_last_not_of(whitespace);
    return text.substr(first, last - first + 1);
}

// The fields of a line, split at every `sep`, EMPTY fields included, as views into the line:
//   split_fields("a,,b", ',') is {"a", "", "b"};  split_fields("", ',') is {""} (one empty field);  split_fields("a,", ',') is {"a", ""}.
// The views are only valid while `line` is.
inline std::vector<std::string_view> split_fields(std::string_view line, char sep) {
    std::vector<std::string_view> fields;
    std::size_t start = 0;
    while (true) {
        const std::size_t at = line.find(sep, start);
        if (at == std::string_view::npos) {
            fields.push_back(line.substr(start));
            return fields;
        }
        fields.push_back(line.substr(start, at - start));
        start = at + 1;
    }
}

// ---- 5. Putting it together: a log-line parser --------------------------------------------------------------------------------------------------------------
// A line is "LEVEL CODE message": LEVEL is DEBUG, INFO, WARN or ERROR, CODE an integer, then the message (everything after the single space that
// follows the code; it may be empty and may contain spaces). Whitespace around the whole line (for example the '\r' of a Windows line ending) is
// ignored. Anything else is not a log line: nothing.
//   parse_log_line("ERROR 503 Service unavailable") is {"ERROR", 503, "Service unavailable"}
// Parse with string_view and from_chars: the only allocations should be the two strings of the result.
struct LogEntry {
    std::string level;
    int code = 0;
    std::string message;
};

inline std::optional<LogEntry> parse_log_line(std::string_view line) {
    line = trim(line);
    const std::size_t first_space = line.find(' ');
    if (first_space == std::string_view::npos) return std::nullopt;
    const std::string_view level = line.substr(0, first_space);
    if (level != "DEBUG" && level != "INFO" && level != "WARN" && level != "ERROR" && level != "error") return std::nullopt;

    const std::string_view rest = line.substr(first_space + 1);
    const std::size_t second_space = rest.find(' ');
    const std::string_view code_text = rest.substr(0, second_space);  // the whole rest when there is no second space
    const std::optional<int> code = parse_int(code_text);
    if (!code) return std::nullopt;

    LogEntry entry;
    entry.level = std::string(level);
    entry.code = *code;
    if (second_space != std::string_view::npos) entry.message = std::string(rest.substr(second_space + 1));
    return entry;
}
