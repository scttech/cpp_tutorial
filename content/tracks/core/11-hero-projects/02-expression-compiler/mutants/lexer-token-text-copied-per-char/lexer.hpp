// MUTANT: a name is built one character at a time
#pragma once
// tokenize(text): one pass over the text, no copies of it; errors are values with the offset of the problem.
#include <cmath>
#include <cstdlib>
#include <string>
#include <string_view>
#include <vector>

#include "types.hpp"

namespace expr {
namespace detail {

inline bool is_digit(char c) { return c >= '0' && c <= '9'; }
inline bool is_name_start(char c) { return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_'; }
inline bool is_name_char(char c) { return is_name_start(c) || is_digit(c); }

inline TokenizeResult lex_error(Error e, std::size_t pos, const char* message) {
    TokenizeResult r;
    r.error = e;
    r.pos = pos;
    r.message = message;
    return r;
}

}  // namespace detail

inline TokenizeResult tokenize(std::string_view s) {
    TokenizeResult r;
    const std::size_t n = s.size();
    std::size_t i = 0;
    while (i < n) {
        const char c = s[i];
        if (c == ' ' || c == '\t' || c == '\n' || c == '\r') {
            ++i;
            continue;
        }
        Token t;
        t.pos = i;
        if (detail::is_digit(c)) {
            const std::size_t start = i;
            while (i < n && detail::is_digit(s[i])) ++i;
            if (i < n && s[i] == '.') {
                ++i;
                if (i >= n || !detail::is_digit(s[i])) return detail::lex_error(Error::BadNumber, i, "digits are needed after the decimal point");
                while (i < n && detail::is_digit(s[i])) ++i;
            }
            if (i < n && (s[i] == 'e' || s[i] == 'E')) {
                ++i;
                if (i < n && (s[i] == '+' || s[i] == '-')) ++i;
                if (i >= n || !detail::is_digit(s[i])) return detail::lex_error(Error::BadNumber, i, "digits are needed in the exponent");
                while (i < n && detail::is_digit(s[i])) ++i;
            }
            const std::string text(s.substr(start, i - start));
            t.kind = Kind::Number;
            t.number = std::strtod(text.c_str(), nullptr);
            if (!std::isfinite(t.number)) return detail::lex_error(Error::BadNumber, start, "the number is too large");
        } else if (detail::is_name_start(c)) {
            const std::size_t start = i;
            while (i < n && detail::is_name_char(s[i])) ++i;
            t.kind = Kind::Ident;
            for (std::size_t k = start; k < i; ++k) t.text = t.text + std::string(1, s[k]);
        } else {
            switch (c) {
                case '+': t.kind = Kind::Plus; break;
                case '-': t.kind = Kind::Minus; break;
                case '*': t.kind = Kind::Star; break;
                case '/': t.kind = Kind::Slash; break;
                case '%': t.kind = Kind::Percent; break;
                case '^': t.kind = Kind::Caret; break;
                case '(': t.kind = Kind::LParen; break;
                case ')': t.kind = Kind::RParen; break;
                case ',': t.kind = Kind::Comma; break;
                default: return detail::lex_error(Error::BadCharacter, i, "unexpected character");
            }
            ++i;
        }
        r.tokens.push_back(std::move(t));
    }
    Token end;
    end.kind = Kind::End;
    end.pos = n;
    r.tokens.push_back(std::move(end));
    return r;
}

}  // namespace expr
