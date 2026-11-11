#pragma once
// YOUR PARSER. parse(text) turns JSON text into a Value, or reports the first error with its line and column.
// The skeleton below knows null/true/false and whitespace; the rest is yours (see the lesson for the grammar).
#include <cstdlib>
#include <string>
#include <string_view>

#include "value.hpp"

namespace json {
namespace detail {

class Parser {
public:
    explicit Parser(std::string_view text) : s_(text) {}

    ParseResult run() {
        skip_ws();
        Value v;
        bool good = parse_value(v, 0);
        if (good) {
            skip_ws();
            if (i_ < s_.size()) good = fail(Error::TrailingContent, "unexpected characters after the document", i_);
        }
        ParseResult r;
        if (good) {
            r.value = std::move(v);
        } else {
            r.error = err_;
            r.message = msg_;
            r.line = 1;  // TODO: the real line and column of err_pos_ (1-based; a '\n' starts a new line)
            r.column = err_pos_ + 1;
        }
        return r;
    }

private:
    std::string_view s_;
    std::size_t i_ = 0;
    Error err_ = Error::None;
    std::size_t err_pos_ = 0;
    std::string msg_;

    bool fail(Error e, std::string message, std::size_t pos) {
        err_ = e;
        msg_ = std::move(message);
        err_pos_ = pos;
        return false;
    }
    bool fail_here(Error e, std::string message) { return fail(e, std::move(message), i_); }
    bool at_end() const { return i_ >= s_.size(); }

    void skip_ws() {
        while (i_ < s_.size() && (s_[i_] == ' ' || s_[i_] == '\t' || s_[i_] == '\n' || s_[i_] == '\r')) ++i_;
    }

    bool literal(std::string_view word, Value v, Value& out) {
        for (char c : word) {
            if (at_end()) return fail_here(Error::UnexpectedEnd, "the text ended inside a word");
            if (s_[i_] != c) return fail_here(Error::UnexpectedCharacter, "not a valid word");
            ++i_;
        }
        out = std::move(v);
        return true;
    }

    bool parse_value(Value& out, int depth) {
        (void)depth;
        if (at_end()) return fail_here(Error::UnexpectedEnd, "expected a value but the text ended");
        switch (s_[i_]) {
            case 'n': return literal("null", Value(nullptr), out);
            case 't': return literal("true", Value(true), out);
            case 'f': return literal("false", Value(false), out);
            // TODO: numbers (also starting with '-'), strings, arrays and objects
            default: return fail_here(Error::UnexpectedCharacter, "unexpected character, expected a value");
        }
    }
};

}  // namespace detail

inline ParseResult parse(std::string_view text) {
    detail::Parser parser(text);
    return parser.run();
}

}  // namespace json
