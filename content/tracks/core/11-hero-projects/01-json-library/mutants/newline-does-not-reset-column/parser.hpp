// MUTANT: the column keeps counting across lines
#pragma once
// parse(text): recursive descent over a std::string_view. Errors are values (Error + line + column), never exceptions.
#include <cstdlib>
#include <string>
#include <string_view>

#include "value.hpp"

namespace json {
namespace detail {

inline bool is_digit(char c) { return c >= '0' && c <= '9'; }

inline int hex_value(char c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
}

inline void append_utf8(std::string& out, unsigned code) {
    if (code < 0x80) {
        out.push_back(static_cast<char>(code));
    } else if (code < 0x800) {
        out.push_back(static_cast<char>(0xC0 | (code >> 6)));
        out.push_back(static_cast<char>(0x80 | (code & 0x3F)));
    } else if (code < 0x10000) {
        out.push_back(static_cast<char>(0xE0 | (code >> 12)));
        out.push_back(static_cast<char>(0x80 | ((code >> 6) & 0x3F)));
        out.push_back(static_cast<char>(0x80 | (code & 0x3F)));
    } else {
        out.push_back(static_cast<char>(0xF0 | (code >> 18)));
        out.push_back(static_cast<char>(0x80 | ((code >> 12) & 0x3F)));
        out.push_back(static_cast<char>(0x80 | ((code >> 6) & 0x3F)));
        out.push_back(static_cast<char>(0x80 | (code & 0x3F)));
    }
}

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
            r.line = 1;
            r.column = 1;
            for (std::size_t k = 0; k < err_pos_; ++k) {
                if (s_[k] == '\n') {
                    ++r.line;
                } else {
                    ++r.column;
                }
            }
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
    bool at_digit() const { return i_ < s_.size() && is_digit(s_[i_]); }

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
        if (at_end()) return fail_here(Error::UnexpectedEnd, "expected a value but the text ended");
        const char c = s_[i_];
        switch (c) {
            case 'n': return literal("null", Value(nullptr), out);
            case 't': return literal("true", Value(true), out);
            case 'f': return literal("false", Value(false), out);
            case '"': {
                std::string str;
                if (!parse_string(str)) return false;
                out = Value(std::move(str));
                return true;
            }
            case '[': return parse_array(out, depth);
            case '{': return parse_object(out, depth);
            default:
                if (c == '-' || is_digit(c)) return parse_number(out);
                return fail_here(Error::UnexpectedCharacter, "unexpected character, expected a value");
        }
    }

    bool parse_number(Value& out) {
        const std::size_t start = i_;
        if (s_[i_] == '-') ++i_;
        if (!at_digit()) return fail_here(Error::BadNumber, "a digit is needed here");
        if (s_[i_] == '0') {
            ++i_;  // a leading zero stands alone: "01" is the number 0 followed by garbage
        } else {
            while (at_digit()) ++i_;
        }
        if (i_ < s_.size() && s_[i_] == '.') {
            ++i_;
            if (!at_digit()) return fail_here(Error::BadNumber, "digits are needed after the decimal point");
            while (at_digit()) ++i_;
        }
        if (i_ < s_.size() && (s_[i_] == 'e' || s_[i_] == 'E')) {
            ++i_;
            if (i_ < s_.size() && (s_[i_] == '+' || s_[i_] == '-')) ++i_;
            if (!at_digit()) return fail_here(Error::BadNumber, "digits are needed in the exponent");
            while (at_digit()) ++i_;
        }
        const std::string token(s_.substr(start, i_ - start));
        const double d = std::strtod(token.c_str(), nullptr);
        if (d > 1.7976931348623157e308 || d < -1.7976931348623157e308) {
            return fail(Error::NumberOutOfRange, "the number does not fit in a double", start);
        }
        out = Value(d);
        return true;
    }

    // four hex digits after "\u"; false (and an error) otherwise
    bool parse_hex4(unsigned& code, std::size_t backslash) {
        code = 0;
        for (int k = 0; k < 4; ++k) {
            if (at_end()) return fail_here(Error::UnexpectedEnd, "the text ended inside a \\u escape");
            const int h = hex_value(s_[i_]);
            if (h < 0) return fail(Error::BadEscape, "\\u needs four hex digits", backslash);
            code = code * 16 + static_cast<unsigned>(h);
            ++i_;
        }
        return true;
    }

    bool parse_escape(std::string& out) {
        const std::size_t backslash = i_;
        ++i_;
        if (at_end()) return fail_here(Error::UnexpectedEnd, "the text ended after a backslash");
        const char c = s_[i_++];
        switch (c) {
            case '"': out.push_back('"'); return true;
            case '\\': out.push_back('\\'); return true;
            case '/': out.push_back('/'); return true;
            case 'b': out.push_back('\b'); return true;
            case 'f': out.push_back('\f'); return true;
            case 'n': out.push_back('\n'); return true;
            case 'r': out.push_back('\r'); return true;
            case 't': out.push_back('\t'); return true;
            case 'u': {
                unsigned code = 0;
                if (!parse_hex4(code, backslash)) return false;
                if (code >= 0xDC00 && code <= 0xDFFF) return fail(Error::BadEscape, "a low surrogate without a high surrogate", backslash);
                if (code >= 0xD800 && code <= 0xDBFF) {
                    if (at_end()) return fail_here(Error::UnexpectedEnd, "the text ended after a high surrogate");
                    if (s_[i_] != '\\' || i_ + 1 >= s_.size() || s_[i_ + 1] != 'u') {
                        return fail(Error::BadEscape, "a high surrogate must be followed by \\u and a low surrogate", backslash);
                    }
                    i_ += 2;
                    unsigned low = 0;
                    if (!parse_hex4(low, backslash)) return false;
                    if (low < 0xDC00 || low > 0xDFFF) return fail(Error::BadEscape, "a high surrogate must be followed by a low surrogate", backslash);
                    code = 0x10000 + ((code - 0xD800) << 10) + (low - 0xDC00);
                }
                append_utf8(out, code);
                return true;
            }
            default: return fail(Error::BadEscape, "unknown escape", backslash);
        }
    }

    bool parse_string(std::string& out) {
        ++i_;  // the opening quote
        while (true) {
            const std::size_t run = i_;
            while (i_ < s_.size() && s_[i_] != '"' && s_[i_] != '\\' && static_cast<unsigned char>(s_[i_]) >= 0x20) ++i_;
            out.append(s_.data() + run, i_ - run);
            if (at_end()) return fail_here(Error::UnexpectedEnd, "the string is not closed");
            const char c = s_[i_];
            if (c == '"') {
                ++i_;
                return true;
            }
            if (c == '\\') {
                if (!parse_escape(out)) return false;
            } else {
                return fail_here(Error::BadString, "a control character must be escaped inside a string");
            }
        }
    }

    bool parse_array(Value& out, int depth) {
        if (depth >= kMaxDepth) return fail_here(Error::TooDeep, "arrays and objects are nested too deeply");
        ++i_;
        Array items;
        skip_ws();
        if (!at_end() && s_[i_] == ']') {
            ++i_;
            out = Value(std::move(items));
            return true;
        }
        while (true) {
            skip_ws();
            Value element;
            if (!parse_value(element, depth + 1)) return false;
            items.push_back(std::move(element));
            skip_ws();
            if (at_end()) return fail_here(Error::UnexpectedEnd, "expected ',' or ']'");
            if (s_[i_] == ',') {
                ++i_;
                continue;
            }
            if (s_[i_] == ']') {
                ++i_;
                break;
            }
            return fail_here(Error::UnexpectedCharacter, "expected ',' or ']'");
        }
        out = Value(std::move(items));
        return true;
    }

    bool parse_object(Value& out, int depth) {
        if (depth >= kMaxDepth) return fail_here(Error::TooDeep, "arrays and objects are nested too deeply");
        ++i_;
        Object members;
        skip_ws();
        if (!at_end() && s_[i_] == '}') {
            ++i_;
            out = Value(std::move(members));
            return true;
        }
        while (true) {
            skip_ws();
            if (at_end()) return fail_here(Error::UnexpectedEnd, "expected a string key");
            if (s_[i_] != '"') return fail_here(Error::UnexpectedCharacter, "an object key must be a string");
            std::string key;
            if (!parse_string(key)) return false;
            skip_ws();
            if (at_end()) return fail_here(Error::UnexpectedEnd, "expected ':'");
            if (s_[i_] != ':') return fail_here(Error::UnexpectedCharacter, "expected ':'");
            ++i_;
            skip_ws();
            Value element;
            if (!parse_value(element, depth + 1)) return false;
            members.insert_or_assign(std::move(key), std::move(element));  // a repeated key: the last one wins
            skip_ws();
            if (at_end()) return fail_here(Error::UnexpectedEnd, "expected ',' or '}'");
            if (s_[i_] == ',') {
                ++i_;
                continue;
            }
            if (s_[i_] == '}') {
                ++i_;
                break;
            }
            return fail_here(Error::UnexpectedCharacter, "expected ',' or '}'");
        }
        out = Value(std::move(members));
        return true;
    }
};

}  // namespace detail

inline ParseResult parse(std::string_view text) {
    detail::Parser parser(text);
    return parser.run();
}

}  // namespace json
