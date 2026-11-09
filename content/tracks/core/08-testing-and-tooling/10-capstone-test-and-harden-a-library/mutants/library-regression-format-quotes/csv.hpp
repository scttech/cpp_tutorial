// MUTANT: csv.hpp has the bug "format-quotes" again: the tests or the hidden checks must fail.
#pragma once

#include <cstddef>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

// A small CSV library (comma-separated values). The rules:
//
//   parse_line(line, delimiter)
//     * the fields are separated by the delimiter; empty fields are kept: "a,,c" is {"a", "", "c"} and "a,b," is {"a", "b", ""}
//     * the empty line is ONE empty field: {""}
//     * a field may be QUOTED: "..." The quotes are removed; the delimiter inside them is data, and two quotes in a row ("") are one quote
//     * a quote in the middle of an UNQUOTED field is just a character
//     * a quoted field that is never closed: csv::ParseError. Anything but the delimiter or the end after a closing quote: csv::ParseError.
//     * a line ending ("\n" or "\r\n") at the end of the line is not part of the last field
//
//   format_line(row, delimiter)
//     * the fields joined by the delimiter. A field that contains the delimiter, a quote, '\r' or '\n' is quoted, and its quotes are doubled.
//     * round trip: parse_line(format_line(row, d), d) == row for every non-empty row. (An empty row formats as "".)
//
//   parse_document(text, delimiter)
//     * the records of a whole text. A line break ("\n" or "\r\n") ends a record, EXCEPT inside quotes, where it is data.
//     * blank lines are skipped; a missing final line break is fine. A quote that is never closed: csv::ParseError.

namespace csv {

class ParseError : public std::runtime_error {
public:
    using std::runtime_error::runtime_error;
};

using Row = std::vector<std::string>;

namespace detail {

inline std::string_view without_line_end(std::string_view line) {
    if (!line.empty() && line.back() == '\n') line.remove_suffix(1);
    if (!line.empty() && line.back() == '\r') line.remove_suffix(1);
    return line;
}

inline bool needs_quotes(std::string_view field, char delimiter) {
    return field.find_first_of(std::string{delimiter, '"', '\r', '\n'}) != std::string_view::npos;
}

}  // namespace detail

inline Row parse_line(std::string_view line, char delimiter = ',') {
    line = detail::without_line_end(line);
    Row row;
    std::string field;
    std::size_t i = 0;
    while (true) {
        field.clear();
        if (i < line.size() && line[i] == '"') {
            ++i;  // the opening quote
            bool closed = false;
            while (i < line.size()) {
                if (line[i] == '"') {
                    if (i + 1 < line.size() && line[i + 1] == '"') {
                        field += '"';
                        i += 2;
                        continue;
                    }
                    closed = true;
                    ++i;
                    break;
                }
                field += line[i++];
            }
            if (!closed) throw ParseError("unterminated quote");
            if (i < line.size() && line[i] != delimiter) throw ParseError("unexpected character after closing quote");
        } else {
            while (i < line.size() && line[i] != delimiter) field += line[i++];
        }
        row.push_back(std::move(field));
        if (i >= line.size()) break;
        ++i;  // the delimiter; when it was the last character the loop runs once more and adds the empty last field
    }
    return row;
}

inline std::string format_line(const Row& row, char delimiter = ',') {
    std::string out;
    for (std::size_t i = 0; i < row.size(); ++i) {
        if (i != 0) out += delimiter;
        const std::string& field = row[i];
        if (detail::needs_quotes(field, delimiter)) {
            out += '"';
            out += field;
            out += '"';
        } else {
            out += field;
        }
    }
    return out;
}

inline std::vector<Row> parse_document(std::string_view text, char delimiter = ',') {
    std::vector<Row> rows;
    std::size_t start = 0;
    bool in_quotes = false;
    for (std::size_t i = 0; i <= text.size(); ++i) {
        const bool at_end = i == text.size();
        if (!at_end && text[i] == '"') in_quotes = !in_quotes;
        if (at_end || (text[i] == '\n' && !in_quotes)) {
            if (at_end && in_quotes) throw ParseError("unterminated quote");
            const std::string_view line = text.substr(start, i - start);
            if (!detail::without_line_end(line).empty()) rows.push_back(parse_line(line, delimiter));
            start = i + 1;
        }
    }
    return rows;
}

}  // namespace csv
