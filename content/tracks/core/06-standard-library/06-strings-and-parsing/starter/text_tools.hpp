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
// Every function is a stub that compiles but is wrong: replace each body.

// ---- 1. std::from_chars: text to a number ------------------------------------------------------------------------------------------------------
// TODO: the WHOLE text must be a decimal integer: an optional '-', then digits. Nothing else: no '+', no spaces, no trailing characters, no overflow.
//   parse_int("42") is 42, parse_int("-7") is -7, parse_int("007") is 7;  parse_int(""), parse_int("12abc"), parse_int(" 5"), parse_int("+5"),
//   parse_int("99999999999") (too big for an int) are all nothing.
// from_chars reports problems through an error code, not an exception, and tells you where it stopped: you must check BOTH:
//     const auto [stop, ec] = std::from_chars(text.data(), text.data() + text.size(), value);   // ec == std::errc{} and stop == end
inline std::optional<int> parse_int(std::string_view text) {
    (void)text;
    return std::nullopt;
}

// TODO: the same for a floating-point number ("3.5", "-0.25", "1e3", ".5"). from_chars ALSO accepts "inf" and "nan": those are not numbers for us
// (nothing), and neither is a text with anything after the number. (std::isfinite)
inline std::optional<double> parse_double(std::string_view text) {
    (void)text;
    return std::nullopt;
}

// ---- 2. std::to_chars: a number to text -------------------------------------------------------------------------------------------------------------
// TODO: the value in lower-case hexadecimal, with no prefix: to_hex(255) is "ff", to_hex(0) is "0", to_hex(4096) is "1000".
// std::to_chars(first, last, value, 16) writes into a char buffer and returns where it stopped (`.ptr`); build the string from [buffer, ptr).
inline std::string to_hex(unsigned value) {
    (void)value;
    return "";
}

// ---- 3. std::format: type-safe formatting -------------------------------------------------------------------------------------------------------------
// TODO: one table row:  the name LEFT-aligned in 10 columns, '|', the count RIGHT-aligned in 5 columns, '|', the ratio with 2 decimals RIGHT-aligned in 8.
//   format_row("ab", 42, 3.14159) is "ab        |   42|    3.14".   (A name longer than 10 characters is not cut.)
// std::format("{:<10}|{:>5}|{:>8.2f}", name, count, ratio): `<` left, `>` right, then the width, `.2f` fixed with 2 decimals.
inline std::string format_row(std::string_view name, int count, double ratio) {
    (void)name;
    (void)count;
    (void)ratio;
    return "";
}

// ---- 4. std::string_view: slicing text without copying ---------------------------------------------------------------------------------------------------
// TODO: the text without leading and trailing ASCII whitespace (space, tab, \r, \n, \v, \f): a VIEW into the original, no copy.
// (find_first_not_of, find_last_not_of, substr)
inline std::string_view trim(std::string_view text) { return text; }

// TODO: the fields of a line, split at every `sep`, EMPTY fields included, as views into the line:
//   split_fields("a,,b", ',') is {"a", "", "b"};  split_fields("", ',') is {""} (one empty field);  split_fields("a,", ',') is {"a", ""}.
// The views are only valid while `line` is. (find(sep, start), substr)
inline std::vector<std::string_view> split_fields(std::string_view line, char sep) {
    (void)line;
    (void)sep;
    return {};
}

// ---- 5. Putting it together: a log-line parser --------------------------------------------------------------------------------------------------------------
// A line is "LEVEL CODE message": LEVEL is DEBUG, INFO, WARN or ERROR, CODE an integer, then the message (everything after the single space that
// follows the code; it may be empty and may contain spaces). Whitespace around the whole line (for example the '\r' of a Windows line ending) is
// ignored. Anything else is not a log line: nothing.
//   parse_log_line("ERROR 503 Service unavailable") is {"ERROR", 503, "Service unavailable"}
// TODO: parse with string_view and parse_int: the only allocations should be the two strings of the result.
struct LogEntry {
    std::string level;
    int code = 0;
    std::string message;
};

inline std::optional<LogEntry> parse_log_line(std::string_view line) {
    (void)line;
    return std::nullopt;
}
