// MUTANT: DuplicateKey points at the value
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

namespace detail {

struct Pos {
    std::size_t line = 1, column = 1;
};

class Parser {
public:
    explicit Parser(std::string_view s) : s_(s) {}

    ParseResult run() {
        Config config;
        if (s_.substr(0, 3) == "\xEF\xBB\xBF") {
            i_ = 3;
            line_start_ = 3;
        }
        std::string prefix;   // the current section's name, with a trailing dot
        for (;;) {
            skip_inline_space();
            if (eof()) break;
            const char c = peek();
            if (c == '\n' || (c == '\r' && peek(1) == '\n')) {
                newline();
                continue;
            }
            if (c == '#') {
                skip_comment();
                continue;
            }
            if (c == '[') {
                if (!section(config, prefix)) return fail_result();
            } else if (!setting(config, prefix)) {
                return fail_result();
            }
            if (!end_of_line()) return fail_result();
        }
        return ParseResult{std::move(config), std::nullopt};
    }

private:
    // ---- cursor ---------------------------------------------------------------------------------------------------------------
    bool eof() const { return i_ >= s_.size(); }
    char peek(std::size_t ahead = 0) const { return i_ + ahead < s_.size() ? s_[i_ + ahead] : '\0'; }
    Pos here() const { return Pos{line_, i_ - line_start_ + 1}; }
    void advance() { ++i_; }
    void newline() {
        if (peek() == '\r') ++i_;
        ++i_;
        ++line_;
        line_start_ = i_;
    }
    void skip_inline_space() {
        while (!eof() && (peek() == ' ' || peek() == '\t')) ++i_;
    }
    void skip_comment() {
        while (!eof() && peek() != '\n') ++i_;
    }
    bool at_line_end() const { return eof() || peek() == '\n' || (peek() == '\r' && peek(1) == '\n'); }

    bool fail(ErrorKind kind, Pos at, std::string message) {
        error_ = Error{kind, at.line, at.column, std::move(message)};
        return false;
    }
    ParseResult fail_result() { return ParseResult{std::nullopt, std::move(error_)}; }

    // after a complete setting or header: spaces, an optional comment, then the end of the line
    bool end_of_line() {
        skip_inline_space();
        if (eof()) return true;
        if (peek() == '#') {
            skip_comment();
            return true;
        }
        if (at_line_end()) return true;
        return fail(ErrorKind::TrailingGarbage, here(), "unexpected text after the value");
    }

    // ---- keys -----------------------------------------------------------------------------------------------------------------
    static bool bare_char(char c) { return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '_' || c == '-'; }

    bool key_path(std::vector<std::string>& parts, ErrorKind bad) {
        for (;;) {
            skip_inline_space();
            std::string part;
            if (peek() == '"') {
                if (!basic_string(part)) return false;
            } else if (peek() == '\'') {
                if (!literal_string(part)) return false;
            } else {
                while (!eof() && bare_char(peek())) part.push_back(s_[i_++]);
                if (part.empty()) return fail(bad, here(), "a key was expected here");
            }
            parts.push_back(std::move(part));
            skip_inline_space();
            if (peek() == '.') {
                advance();
                continue;
            }
            return true;
        }
    }

    static std::string join(const std::vector<std::string>& parts) {
        std::string out;
        for (std::size_t k = 0; k < parts.size(); ++k) {
            if (k) out += '.';
            out += parts[k];
        }
        return out;
    }

    bool section(Config& config, std::string& prefix) {
        const Pos open = here();
        advance();   // '['
        std::vector<std::string> parts;
        if (!key_path(parts, ErrorKind::InvalidSection)) return false;
        skip_inline_space();
        if (peek() != ']') return fail(ErrorKind::InvalidSection, here(), "a section header must end with ']'");
        advance();
        const std::string name = join(parts);
        if (!config.add_section(name)) return fail(ErrorKind::DuplicateSection, open, "section [" + name + "] is defined twice");
        prefix = name + ".";
        return true;
    }

    bool setting(Config& config, const std::string& prefix) {
        const Pos start = here();
        std::vector<std::string> parts;
        if (!key_path(parts, ErrorKind::InvalidKey)) return false;
        skip_inline_space();
        if (peek() != '=' || eof()) return fail(ErrorKind::ExpectedEquals, here(), "'=' expected after the key");
        advance();
        skip_inline_space();
        Value v;
        if (!value(v)) return false;
        if (!config.add(prefix + join(parts), std::move(v))) return fail(ErrorKind::DuplicateKey, here(), "the key '" + prefix + join(parts) + "' is defined twice");
        return true;
    }

    // ---- values ---------------------------------------------------------------------------------------------------------------
    bool value(Value& out) {
        const char c = peek();
        if (eof() || at_line_end() || c == ',' || c == ']' || c == '#') return fail(ErrorKind::ExpectedValue, here(), "a value was expected here");
        if (c == '"') {
            std::string s;
            if (!basic_string(s)) return false;
            out = Value::str(std::move(s));
            return true;
        }
        if (c == '\'') {
            std::string s;
            if (!literal_string(s)) return false;
            out = Value::str(std::move(s));
            return true;
        }
        if (c == '[') return array(out);
        const Pos at = here();
        const std::size_t begin = i_;
        while (!eof() && !is_token_end(peek())) ++i_;
        const std::string_view token = s_.substr(begin, i_ - begin);
        if (token == "true" || token == "false") {
            out = Value::boolean(token == "true");
            return true;
        }
        if ((token[0] >= '0' && token[0] <= '9') || token[0] == '+' || token[0] == '-' || token[0] == '.') return number(token, at, out);
        return fail(ErrorKind::InvalidValue, at, "'" + std::string(token) + "' is not a value (strings need quotes)");
    }

    static bool is_token_end(char c) { return c == ' ' || c == '\t' || c == '\r' || c == '\n' || c == ',' || c == ']' || c == '#'; }

    bool array(Value& out) {
        const Pos open = here();
        advance();   // '['
        std::vector<Value> items;
        for (;;) {
            if (!skip_array_space()) return fail(ErrorKind::UnterminatedArray, open, "the array is never closed");
            if (peek() == ']') {
                advance();
                break;
            }
            const Pos at = here();
            Value item;
            if (!value(item)) return false;
            if (!items.empty() && items.front().type != item.type) return fail(ErrorKind::MixedArray, at, "all elements of an array must have the same type");
            items.push_back(std::move(item));
            if (!skip_array_space()) return fail(ErrorKind::UnterminatedArray, open, "the array is never closed");
            if (peek() == ',') {
                advance();
                continue;
            }
            if (peek() == ']') {
                advance();
                break;
            }
            return fail(ErrorKind::UnexpectedCharacter, here(), "',' or ']' expected in the array");
        }
        out = Value::array(std::move(items));
        return true;
    }

    // whitespace, newlines and comments between array elements; false at the end of the text
    bool skip_array_space() {
        for (;;) {
            skip_inline_space();
            if (eof()) return false;
            if (peek() == '#') {
                skip_comment();
                continue;
            }
            if (peek() == '\n' || (peek() == '\r' && peek(1) == '\n')) {
                newline();
                continue;
            }
            return true;
        }
    }

    // ---- strings --------------------------------------------------------------------------------------------------------------
    static bool is_control(unsigned char c) { return (c < 0x20 && c != '\t') || c == 0x7F; }

    bool literal_string(std::string& out) {
        const Pos open = here();
        advance();
        for (;;) {
            if (eof() || at_line_end()) return fail(ErrorKind::UnterminatedString, open, "the string is not closed on its line");
            const char c = peek();
            if (c == '\'') {
                advance();
                return true;
            }
            if (is_control(static_cast<unsigned char>(c))) return fail(ErrorKind::ControlCharacter, here(), "a control character is not allowed in a string");
            out.push_back(c);
            advance();
        }
    }

    static int hex_value(char c) {
        if (c >= '0' && c <= '9') return c - '0';
        if (c >= 'a' && c <= 'f') return c - 'a' + 10;
        if (c >= 'A' && c <= 'F') return c - 'A' + 10;
        return -1;
    }

    static void append_utf8(std::string& out, std::uint32_t cp) {
        if (cp < 0x80) {
            out.push_back(static_cast<char>(cp));
        } else if (cp < 0x800) {
            out.push_back(static_cast<char>(0xC0 | (cp >> 6)));
            out.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
        } else {
            out.push_back(static_cast<char>(0xE0 | (cp >> 12)));
            out.push_back(static_cast<char>(0x80 | ((cp >> 6) & 0x3F)));
            out.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
        }
    }

    bool basic_string(std::string& out) {
        const Pos open = here();
        advance();
        for (;;) {
            if (eof() || at_line_end()) return fail(ErrorKind::UnterminatedString, open, "the string is not closed on its line");
            const char c = peek();
            if (c == '"') {
                advance();
                return true;
            }
            if (c != '\\') {
                if (is_control(static_cast<unsigned char>(c))) return fail(ErrorKind::ControlCharacter, here(), "a control character is not allowed in a string");
                out.push_back(c);
                advance();
                continue;
            }
            const Pos slash = here();
            advance();
            const char e = peek();
            switch (e) {
                case '"': out.push_back('"'); break;
                case '\\': out.push_back('\\'); break;
                case 'n': out.push_back('\n'); break;
                case 't': out.push_back('\t'); break;
                case 'r': out.push_back('\r'); break;
                case 'b': out.push_back('\b'); break;
                case 'f': out.push_back('\f'); break;
                case 'u': {
                    std::uint32_t cp = 0;
                    for (int k = 1; k <= 4; ++k) {
                        const int h = hex_value(peek(static_cast<std::size_t>(k)));
                        if (h < 0) return fail(ErrorKind::InvalidEscape, slash, "\\u needs four hexadecimal digits");
                        cp = cp * 16 + static_cast<std::uint32_t>(h);
                    }
                    if (cp >= 0xD800 && cp <= 0xDFFF) return fail(ErrorKind::InvalidEscape, slash, "\\u must not name a surrogate");
                    append_utf8(out, cp);
                    i_ += 4;
                    break;
                }
                default: return fail(ErrorKind::InvalidEscape, slash, "unknown escape sequence");
            }
            advance();
        }
    }

    // ---- numbers --------------------------------------------------------------------------------------------------------------
    // digits with single underscores between them; returns false if the grammar is broken. `begin` advances.
    static bool digits(std::string_view t, std::size_t& p, std::string& out, bool hex, bool allow_leading_zero) {
        const std::size_t start = p;
        bool last_digit = false;
        while (p < t.size()) {
            const char c = t[p];
            const bool is_digit = hex ? hex_value(c) >= 0 : (c >= '0' && c <= '9');
            if (is_digit) {
                out.push_back(c);
                last_digit = true;
            } else if (c == '_') {
                if (!last_digit) return false;
                last_digit = false;
            } else {
                break;
            }
            ++p;
        }
        if (p == start || !last_digit) return false;   // empty, or ends with an underscore
        if (!allow_leading_zero && out.size() > 1 && out[0] == '0') return false;
        return true;
    }

    bool number(std::string_view t, Pos at, Value& out) {
        const auto bad = [&]() { return fail(ErrorKind::InvalidNumber, at, "'" + std::string(t) + "' is not a valid number"); };
        std::size_t p = 0;
        bool negative = false;
        if (t[p] == '+' || t[p] == '-') {
            negative = t[p] == '-';
            ++p;
        }
        if (p + 1 < t.size() && t[p] == '0' && (t[p + 1] == 'x' || t[p + 1] == 'X')) {
            if (p != 0) return bad();   // hex has no sign
            p += 2;
            std::string d;
            if (!digits(t, p, d, true, true) || p != t.size()) return bad();
            unsigned long long v = 0;
            const auto r = std::from_chars(d.data(), d.data() + d.size(), v, 16);
            if (r.ec != std::errc() || v > static_cast<unsigned long long>(INT64_MAX)) return bad();
            out = Value::integer(static_cast<long long>(v));
            return true;
        }
        std::string whole;
        if (!digits(t, p, whole, false, false)) return bad();
        std::string text = negative ? "-" + whole : whole;
        bool is_float = false;
        if (p < t.size() && t[p] == '.') {
            ++p;
            std::string frac;
            if (!digits(t, p, frac, false, true)) return bad();
            text += "." + frac;
            is_float = true;
        }
        if (p < t.size() && (t[p] == 'e' || t[p] == 'E')) {
            ++p;
            std::string sign;
            if (p < t.size() && (t[p] == '+' || t[p] == '-')) sign = std::string(1, t[p++]);
            std::string exp;
            if (!digits(t, p, exp, false, true)) return bad();
            text += "e" + sign + exp;
            is_float = true;
        }
        if (p != t.size()) return bad();
        if (is_float) {
            double v = 0;
            const auto r = std::from_chars(text.data(), text.data() + text.size(), v);
            if (r.ec == std::errc::result_out_of_range || (r.ec == std::errc() && !std::isfinite(v))) return bad();
            if (r.ec != std::errc() || r.ptr != text.data() + text.size()) return bad();
            out = Value::real(v);
            return true;
        }
        long long v = 0;
        const auto r = std::from_chars(text.data(), text.data() + text.size(), v);
        if (r.ec != std::errc() || r.ptr != text.data() + text.size()) return bad();
        out = Value::integer(v);
        return true;
    }

    std::string_view s_;
    std::size_t i_ = 0;
    std::size_t line_ = 1;
    std::size_t line_start_ = 0;
    std::optional<Error> error_;
};

}  // namespace detail

inline ParseResult parse(std::string_view text) { return detail::Parser(text).run(); }

inline std::string render(std::string_view source, const Error& e, std::string_view filename) {
    std::string out = std::string(filename) + ":" + std::to_string(e.line) + ":" + std::to_string(e.column) + ": error: " + e.message + "\n";
    // find the source line
    std::size_t pos = 0;
    for (std::size_t n = 1; n < e.line && pos < source.size(); ++n) {
        const std::size_t nl = source.find('\n', pos);
        if (nl == std::string_view::npos) {
            pos = source.size();
            break;
        }
        pos = nl + 1;
    }
    std::string_view line = source.substr(std::min(pos, source.size()));
    const std::size_t end = line.find('\n');
    if (end != std::string_view::npos) line = line.substr(0, end);
    if (!line.empty() && line.back() == '\r') line.remove_suffix(1);
    const std::string gutter = std::to_string(e.line);
    out += gutter + " | " + std::string(line) + "\n";
    out += std::string(gutter.size(), ' ') + " | ";
    for (std::size_t k = 0; k + 1 < e.column; ++k) out += (k < line.size() && line[k] == '\t') ? '\t' : ' ';
    out += "^\n";
    return out;
}

}  // namespace conf
