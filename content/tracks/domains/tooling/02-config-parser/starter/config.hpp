#pragma once
// YOUR CONFIGURATION PARSER (a TOML-like subset). namespace conf; Value and Config are in types.hpp.
//
// THE FORMAT
//   # a comment runs to the end of the line (it may follow a value); blank lines are ignored; lines end in "\n" or "\r\n"; a UTF-8 byte order mark at the start of the text is skipped
//   [section]      a section header; the name is one or more keys joined with dots: [server.tls]. Spaces/tabs are allowed inside the brackets and around dots. Everything below belongs to it.
//   key = value    a setting. The key is one or more parts joined with dots: a.b = 1. In a section "s" the full path is "s.a.b", at the top of the file just "a.b". Spaces/tabs around '=' and the dots are fine.
//   A key PART is either bare (letters, digits, '_' and '-', at least one) or a quoted string ("my key" or 'my key').
//   VALUES
//     "basic string"   with escapes \" \\ \n \t \r \b \f and \uXXXX (4 hex digits, a Unicode scalar value that is not a surrogate, written out as UTF-8)
//     'literal string' no escapes at all
//     Both end on the same line. A raw control character (below 0x20 except tab, or 0x7F) inside a string is an error. Bytes of 0x80 and above are passed through untouched (the text is assumed to be UTF-8, not validated).
//     true / false     exactly these words
//     integers         -17, +5, 1_000_000, 0xFF_AA (hex: lower or upper case, no sign). Underscores only BETWEEN digits ("1_000" yes, "_1", "1_", "1__0" no). No leading zeros ("007" is an error, "0" is fine).
//                      The value must fit in long long (-9223372036854775808 is fine, 9223372036854775808 is not).
//     floats           a decimal number with a fraction and/or an exponent: 3.14, -0.5, 1e10, 2.5E-3, 6.02e+23, 1_0.5. Digits are required on both sides of the '.' ("1." and ".5" are errors) and after 'e'.
//                      A result that overflows to infinity (1e999) is an error. inf and nan are not accepted.
//     arrays           [v, v, v]: elements are separated by commas, a trailing comma is allowed, [] is empty, newlines and comments are allowed between elements. All elements must have the same type
//                      (an int and a float are different types; arrays of arrays are fine, their contents need not match each other).
//
// parse(text) -> ParseResult { optional<Config> config; optional<Error> error; }   exactly one is set. The FIRST error stops the parse.
// Error { ErrorKind kind; size_t line; size_t column; std::string message; }   line and column are 1-based; the column counts BYTES from the start of the line (a tab is one column; a BOM is not counted).
//   WHERE each kind points (this is the contract the tests check):
//     UnexpectedCharacter  the offending character (e.g. a character that is neither ',' nor ']' inside an array)
//     InvalidKey           the offending character in a key (an empty part, such as the second '.' of "a..b", or a character that cannot be in a bare key); inside [ ] headers the kind is InvalidSection instead
//     InvalidSection       the offending character in a section header, or the end of the line if the ']' is missing
//     ExpectedEquals       where the '=' should be (the offending character, or the end of the line)
//     ExpectedValue        where the value should start (the offending character, or the end of the line); also an empty element in an array
//     InvalidValue         the first character of a bare word that is not true, false or a number
//     InvalidNumber        the first character of the number
//     UnterminatedString   the opening quote
//     InvalidEscape        the backslash
//     ControlCharacter     the character (also a lone '\r' that is not followed by '\n')
//     UnterminatedArray    the opening '[' of the array (the text ended inside it)
//     MixedArray           the first element whose type differs from the first element's
//     TrailingGarbage      the first character after a complete value or section header (that is not a comment)
//     DuplicateKey         the first character of the key in the second definition
//     DuplicateSection     the '[' of the second header for the same section
//   message: a short human sentence that mentions what was wrong (any wording).
//
// render(source, error, filename) -> string     a compiler-style diagnostic:
//     "<filename>:<line>:<column>: error: <message>\n"
//     "<line> | <the source line, without its line ending>\n"
//     "<spaces as wide as the line number> | <for each byte before the column: a tab if the source has a tab there, else a space>^\n"
//   If the line number is beyond the end of the text the source line is empty.
#include <algorithm>
#include <charconv>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "types.hpp"

namespace conf {

enum class ErrorKind {
    UnexpectedCharacter, InvalidKey, InvalidSection, ExpectedEquals, ExpectedValue, InvalidValue, InvalidNumber, UnterminatedString, InvalidEscape,
    ControlCharacter, UnterminatedArray, MixedArray, TrailingGarbage, DuplicateKey, DuplicateSection
};

struct Error {
    ErrorKind kind;
    std::size_t line;
    std::size_t column;
    std::string message;
};

struct ParseResult {
    std::optional<Config> config;
    std::optional<Error> error;
};

inline ParseResult parse(std::string_view) { return ParseResult{}; }   // TODO
inline std::string render(std::string_view, const Error&, std::string_view) { return ""; }   // TODO

}  // namespace conf
