// MUTANT: extra arguments are ignored
#pragma once
// parse_command(line): split into tokens (bare words and quoted strings), then look at the first one.
#include <string>
#include <string_view>
#include <vector>

#include "types.hpp"

namespace db {
namespace detail {

struct Token {
    std::string text;
    bool quoted = false;
    std::size_t pos = 0;
};

inline bool is_space(char c) { return c == ' ' || c == '\t' || c == '\r' || c == '\n'; }

inline ParsedCommand parse_failure(ParseError e, std::size_t pos, const char* message) {
    ParsedCommand p;
    p.error = e;
    p.pos = pos;
    p.message = message;
    return p;
}

inline char upper(char c) { return (c >= 'a' && c <= 'z') ? static_cast<char>(c - 'a' + 'A') : c; }

inline bool same_word(const std::string& word, std::string_view upper_name) {
    if (word.size() != upper_name.size()) return false;
    for (std::size_t i = 0; i < word.size(); ++i) {
        if (upper(word[i]) != upper_name[i]) return false;
    }
    return true;
}

}  // namespace detail

inline ParsedCommand parse_command(std::string_view line) {
    using detail::Token;
    std::vector<Token> tokens;
    const std::size_t n = line.size();
    std::size_t i = 0;
    while (true) {
        while (i < n && detail::is_space(line[i])) ++i;
        if (i >= n) break;
        Token t;
        t.pos = i;
        if (line[i] == '"') {
            t.quoted = true;
            ++i;
            bool closed = false;
            while (i < n) {
                const std::size_t run = i;
                while (i < n && line[i] != '"' && line[i] != '\\') ++i;
                t.text.append(line.data() + run, i - run);
                if (i >= n) break;
                if (line[i] == '"') {
                    closed = true;
                    ++i;
                    break;
                }
                const std::size_t backslash = i;  // a backslash: look at the next character
                ++i;
                if (i >= n) break;
                switch (line[i]) {
                    case '"': t.text.push_back('"'); break;
                    case '\\': t.text.push_back('\\'); break;
                    case 'n': t.text.push_back('\n'); break;
                    case 't': t.text.push_back('\t'); break;
                    default: return detail::parse_failure(ParseError::BadEscape, backslash, "unknown escape (use \\\" \\\\ \\n or \\t)");
                }
                ++i;
            }
            if (!closed) return detail::parse_failure(ParseError::UnterminatedString, t.pos, "the quoted string is not closed");
            if (i < n && !detail::is_space(line[i])) return detail::parse_failure(ParseError::StrayQuote, i, "a closing quote must be followed by a space or the end");
        } else {
            const std::size_t start = i;
            while (i < n && !detail::is_space(line[i])) {
                if (line[i] == '"') return detail::parse_failure(ParseError::StrayQuote, i, "a quote cannot appear inside a word");
                ++i;
            }
            t.text = std::string(line.substr(start, i - start));
        }
        tokens.push_back(std::move(t));
    }

    if (tokens.empty()) return detail::parse_failure(ParseError::Empty, n, "empty command");
    Token& first = tokens[0];
    const std::size_t args = tokens.size() - 1;
    ParsedCommand p;

    struct Spec { const char* name; std::size_t arity; };
    static constexpr Spec specs[] = {{"SET", 2}, {"GET", 1}, {"DEL", 1}, {"COUNT", 1}, {"RANGE", 2}, {"INCR", 1}, {"BEGIN", 0}, {"ROLLBACK", 0}, {"COMMIT", 0}};
    const Spec* spec = nullptr;
    if (!first.quoted) {
        for (const Spec& s : specs) {
            if (detail::same_word(first.text, s.name)) spec = &s;
        }
    }
    if (!spec) return detail::parse_failure(ParseError::UnknownCommand, first.pos, "unknown command");
    if (args < spec->arity) {
        p.error = ParseError::WrongArity;
        p.pos = first.pos;
        p.message = std::string(spec->name) + " takes " + std::to_string(spec->arity) + " argument(s)";
        return p;
    }

    const std::string_view cmd = spec->name;
    if (cmd == "SET") p.command = Set{Text(std::move(tokens[1].text)), Text(std::move(tokens[2].text))};
    else if (cmd == "GET") p.command = Get{Text(std::move(tokens[1].text))};
    else if (cmd == "DEL") p.command = Del{Text(std::move(tokens[1].text))};
    else if (cmd == "COUNT") p.command = Count{Text(std::move(tokens[1].text))};
    else if (cmd == "RANGE") p.command = Range{Text(std::move(tokens[1].text)), Text(std::move(tokens[2].text))};
    else if (cmd == "INCR") p.command = Incr{Text(std::move(tokens[1].text))};
    else if (cmd == "BEGIN") p.command = Begin{};
    else if (cmd == "ROLLBACK") p.command = Rollback{};
    else p.command = Commit{};
    return p;
}

}  // namespace db
