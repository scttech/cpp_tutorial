#pragma once
// YOUR TOKENIZER. tokenize(text) turns the text into tokens (ending with a Kind::End token), or reports the first error.
// The skeleton skips whitespace and knows the one-character tokens; numbers and identifiers are yours.
#include <cstdlib>
#include <string>
#include <string_view>
#include <vector>

#include "types.hpp"

namespace expr {

inline TokenizeResult tokenize(std::string_view s) {
    TokenizeResult r;
    std::size_t i = 0;
    while (i < s.size()) {
        const char c = s[i];
        if (c == ' ' || c == '\t' || c == '\n' || c == '\r') {
            ++i;
            continue;
        }
        Kind kind = Kind::End;
        switch (c) {
            case '+': kind = Kind::Plus; break;
            case '-': kind = Kind::Minus; break;
            case '*': kind = Kind::Star; break;
            case '/': kind = Kind::Slash; break;
            case '%': kind = Kind::Percent; break;
            case '^': kind = Kind::Caret; break;
            case '(': kind = Kind::LParen; break;
            case ')': kind = Kind::RParen; break;
            case ',': kind = Kind::Comma; break;
            default: break;  // TODO: numbers (1  2.5  1e-3) and identifiers (x  some_name2)
        }
        if (kind == Kind::End) {
            r.tokens.clear();
            r.error = Error::BadCharacter;
            r.pos = i;
            r.message = "unexpected character";
            return r;
        }
        Token t;
        t.kind = kind;
        t.pos = i;
        r.tokens.push_back(std::move(t));
        ++i;
    }
    Token end;
    end.pos = s.size();
    r.tokens.push_back(std::move(end));
    return r;
}

}  // namespace expr
