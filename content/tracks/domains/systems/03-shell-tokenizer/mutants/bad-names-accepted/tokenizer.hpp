// MUTANT: ${1x} and ${} are not rejected
#pragma once
// A single left-to-right pass: a word under construction, a start offset, and one branch per kind of character.
#include <cstddef>
#include <string>
#include <string_view>
#include <vector>

#include "types.hpp"

namespace shell {
namespace detail {

inline bool is_name_start(char c) { return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_'; }
inline bool is_name_char(char c) { return is_name_start(c) || (c >= '0' && c <= '9'); }
inline bool is_operator_char(char c) { return c == '|' || c == '&' || c == ';' || c == '<' || c == '>'; }

inline bool is_valid_name(std::string_view name) {
    if (name.empty() || !is_name_start(name[0])) return false;
    for (char c : name) {
        if (!is_name_char(c)) return false;
    }
    return true;
}

inline void append_value(const Env& env, std::string_view name, std::string& out) {
    const auto it = env.find(std::string(name));
    if (it != env.end()) out += it->second;
}

// line[i] is '$'. Appends what it stands for to `out` and returns the index after the reference; on an error sets `error`.
inline std::size_t expand(std::string_view line, std::size_t i, const Env& env, std::string& out, ErrorKind& error) {
    const std::size_t n = line.size();
    if (i + 1 < n && line[i + 1] == '{') {
        const std::size_t close = line.find('}', i + 2);
        if (close == std::string_view::npos) {
            error = ErrorKind::UnterminatedBrace;
            return i;
        }
        const std::string_view name = line.substr(i + 2, close - i - 2);
        append_value(env, name, out);
        return close + 1;
    }
    if (i + 1 < n && is_name_start(line[i + 1])) {
        std::size_t j = i + 2;
        while (j < n && is_name_char(line[j])) ++j;
        append_value(env, line.substr(i + 1, j - i - 1), out);
        return j;
    }
    out += '$';
    return i + 1;
}

}  // namespace detail

inline Result tokenize(std::string_view line, const Env& env = Env{}) {
    Result r;
    std::string word;
    bool in_word = false;
    std::size_t start = 0;
    const std::size_t n = line.size();

    auto begin_word = [&](std::size_t at) {
        if (!in_word) {
            in_word = true;
            start = at;
        }
    };
    auto flush = [&] {
        if (in_word) {
            r.tokens.push_back(Token{TokenKind::Word, std::move(word), start});
            word.clear();
            in_word = false;
        }
    };
    auto fail = [&](ErrorKind kind, std::size_t at) {
        r.error = kind;
        r.error_pos = at;
    };

    std::size_t i = 0;
    while (i < n) {
        const char c = line[i];
        if (c == ' ' || c == '\t') {
            flush();
            ++i;
        } else if (c == '#' && !in_word) {
            break;  // a comment runs to the end of the line
        } else if (detail::is_operator_char(c)) {
            flush();
            std::size_t len = 1;
            if (i + 1 < n && ((c == '&' && line[i + 1] == '&') || (c == '|' && line[i + 1] == '|') || (c == '>' && line[i + 1] == '>'))) len = 2;
            r.tokens.push_back(Token{TokenKind::Operator, std::string(line.substr(i, len)), i});
            i += len;
        } else if (c == '\\') {
            if (i + 1 == n) {
                fail(ErrorKind::DanglingBackslash, i);
                return r;
            }
            begin_word(i);
            word += line[i + 1];
            i += 2;
        } else if (c == '\'') {
            begin_word(i);
            const std::size_t close = line.find('\'', i + 1);
            if (close == std::string_view::npos) {
                fail(ErrorKind::UnterminatedSingleQuote, i);
                return r;
            }
            word.append(line.substr(i + 1, close - i - 1));
            i = close + 1;
        } else if (c == '"') {
            begin_word(i);
            std::size_t j = i + 1;
            bool closed = false;
            while (j < n) {
                const char d = line[j];
                if (d == '"') {
                    closed = true;
                    ++j;
                    break;
                }
                if (d == '\\' && j + 1 < n && (line[j + 1] == '"' || line[j + 1] == '\\' || line[j + 1] == '$')) {
                    word += line[j + 1];
                    j += 2;
                } else if (d == '$') {
                    ErrorKind error = ErrorKind::None;
                    j = detail::expand(line, j, env, word, error);
                    if (error != ErrorKind::None) {
                        fail(error, j);
                        return r;
                    }
                } else {
                    word += d;
                    ++j;
                }
            }
            if (!closed) {
                fail(ErrorKind::UnterminatedDoubleQuote, i);
                return r;
            }
            i = j;
        } else if (c == '$') {
            ErrorKind error = ErrorKind::None;
            const std::size_t next = detail::expand(line, i, env, word, error);
            if (error != ErrorKind::None) {
                fail(error, i);
                return r;
            }
            if (!word.empty()) begin_word(i);  // an empty expansion adds nothing
            i = next;
        } else {
            begin_word(i);
            word += c;
            ++i;
        }
    }
    flush();
    return r;
}

}  // namespace shell
