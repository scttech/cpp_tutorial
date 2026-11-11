// MUTANT: \d inside a class is the letter d
#pragma once
// parse(pattern): recursive descent, one function per precedence level.
#include <bitset>
#include <string>
#include <string_view>
#include <vector>

#include "types.hpp"

namespace rx {
namespace detail {

inline std::bitset<256> shorthand(char c) {
    std::bitset<256> bits;
    switch (c) {
        case 'd': case 'D':
            for (int ch = '0'; ch <= '9'; ++ch) bits.set(static_cast<std::size_t>(ch));
            break;
        case 'w': case 'W':
            for (int ch = '0'; ch <= '9'; ++ch) bits.set(static_cast<std::size_t>(ch));
            for (int ch = 'a'; ch <= 'z'; ++ch) bits.set(static_cast<std::size_t>(ch));
            for (int ch = 'A'; ch <= 'Z'; ++ch) bits.set(static_cast<std::size_t>(ch));
            bits.set('_');
            break;
        default:  // s, S
            for (const char ch : {' ', '\t', '\n', '\r', '\f', '\v'}) bits.set(static_cast<unsigned char>(ch));
    }
    if (c == 'D' || c == 'W' || c == 'S') bits.flip();
    return bits;
}

inline bool is_shorthand(char c) { return c == 'd' || c == 'w' || c == 's' || c == 'D' || c == 'W' || c == 'S'; }
inline bool is_meta(char c) {
    for (const char m : std::string_view("\\.*+?|()[]^$")) {
        if (c == m) return true;
    }
    return false;
}
inline bool is_quantifier(char c) { return c == '*' || c == '+' || c == '?'; }

class Parser {
public:
    explicit Parser(std::string_view pattern) : p_(pattern) {}

    ParseResult run() {
        ParseResult r;
        NodePtr tree = parse_alt(0);
        if (tree && i_ < p_.size()) tree = fail(CompileError::UnmatchedParen, i_, "there is a ) without a matching (");
        if (tree) {
            r.ast = std::move(tree);
        } else {
            r.error = err_;
            r.pos = pos_;
            r.message = msg_;
        }
        return r;
    }

private:
    std::string_view p_;
    std::size_t i_ = 0;
    CompileError err_ = CompileError::None;
    std::size_t pos_ = 0;
    std::string msg_;

    NodePtr fail(CompileError e, std::size_t pos, const char* message) {
        err_ = e;
        pos_ = pos;
        msg_ = message;
        return nullptr;
    }
    bool more() const { return i_ < p_.size(); }

    NodePtr parse_alt(int depth) {
        std::vector<NodePtr> options;
        while (true) {
            NodePtr branch = parse_concat(depth);
            if (!branch) return nullptr;
            options.push_back(std::move(branch));
            if (more() && p_[i_] == '|') {
                ++i_;
                continue;
            }
            break;
        }
        if (options.size() == 1) return std::move(options[0]);
        return make_alt(std::move(options));
    }

    NodePtr parse_concat(int depth) {
        std::vector<NodePtr> parts;
        while (more() && p_[i_] != '|' && p_[i_] != ')') {
            NodePtr item = parse_postfix(depth);
            if (!item) return nullptr;
            parts.push_back(std::move(item));
        }
        if (parts.empty()) return make_empty();
        if (parts.size() == 1) return std::move(parts[0]);
        return make_concat(std::move(parts));
    }

    NodePtr parse_postfix(int depth) {
        if (is_quantifier(p_[i_])) return fail(CompileError::NothingToRepeat, i_, "a quantifier needs something to repeat");
        NodePtr atom = parse_atom(depth);
        if (!atom) return nullptr;
        const bool anchor = std::holds_alternative<LineStart>(atom->v) || std::holds_alternative<LineEnd>(atom->v);
        int stacked = 0;
        while (more() && is_quantifier(p_[i_])) {
            if (anchor) return fail(CompileError::NothingToRepeat, i_, "^ and $ cannot be repeated");
            if (stacked >= kMaxDepth) return fail(CompileError::TooDeep, i_, "too many quantifiers on one item");
            atom = make_repeat(p_[i_], std::move(atom));
            ++stacked;
            ++i_;
        }
        return atom;
    }

    NodePtr parse_atom(int depth) {
        const char c = p_[i_];
        switch (c) {
            case '(': {
                if (depth >= kMaxDepth) return fail(CompileError::TooDeep, i_, "parentheses are nested too deeply");
                const std::size_t open = i_;
                ++i_;
                NodePtr inner = parse_alt(depth + 1);
                if (!inner) return nullptr;
                if (!more() || p_[i_] != ')') return fail(CompileError::UnmatchedParen, open, "this ( is never closed");
                ++i_;
                return inner;
            }
            case '.': ++i_; return make_any();
            case '^': ++i_; return make_line_start();
            case '$': ++i_; return make_line_end();
            case '[': return parse_class();
            case '\\': return parse_escape();
            default: ++i_; return make_literal(static_cast<unsigned char>(c));
        }
    }

    // after a backslash outside a class
    NodePtr parse_escape() {
        const std::size_t backslash = i_;
        ++i_;
        if (!more()) return fail(CompileError::BadEscape, backslash, "a backslash at the end of the pattern");
        const char c = p_[i_++];
        if (is_meta(c)) return make_literal(static_cast<unsigned char>(c));
        if (is_shorthand(c)) return make_set(shorthand(c));
        if (c == 'n') return make_literal('\n');
        if (c == 't') return make_literal('\t');
        if (c == 'r') return make_literal('\r');
        return fail(CompileError::BadEscape, backslash, "unknown escape");
    }

    // one item of a class: a byte, or (for \d \w \s ...) a set. false: error recorded.
    bool class_item(unsigned char& ch, std::bitset<256>& set, bool& is_set) {
        is_set = false;
        if (p_[i_] != '\\') {
            ch = static_cast<unsigned char>(p_[i_++]);
            return true;
        }
        const std::size_t backslash = i_;
        ++i_;
        if (!more()) return fail(CompileError::BadEscape, backslash, "a backslash at the end of the pattern") != nullptr;
        const char c = p_[i_++];
        if (is_shorthand(c) && c != 'd') {
            set = shorthand(c);
            is_set = true;
            return true;
        }
        if (c == 'd') {
            ch = 'd';
            return true;
        }
        if (is_meta(c) || c == '-') ch = static_cast<unsigned char>(c);
        else if (c == 'n') ch = '\n';
        else if (c == 't') ch = '\t';
        else if (c == 'r') ch = '\r';
        else return fail(CompileError::BadEscape, backslash, "unknown escape") != nullptr;
        return true;
    }

    NodePtr parse_class() {
        const std::size_t open = i_;
        ++i_;
        bool negate = false;
        if (more() && p_[i_] == '^') {
            negate = true;
            ++i_;
        }
        std::bitset<256> bits;
        std::size_t items = 0;
        while (true) {
            if (!more()) return fail(CompileError::BadClass, open, "this [ is never closed");
            if (p_[i_] == ']') {
                ++i_;
                break;
            }
            const std::size_t item_pos = i_;
            unsigned char lo = 0;
            std::bitset<256> set;
            bool is_set = false;
            if (!class_item(lo, set, is_set)) return nullptr;
            ++items;
            if (is_set) {
                bits |= set;
            } else if (i_ + 1 < p_.size() && p_[i_] == '-' && p_[i_ + 1] != ']') {  // a range lo-hi
                ++i_;
                unsigned char hi = 0;
                std::bitset<256> end_set;
                bool end_is_set = false;
                if (!class_item(hi, end_set, end_is_set)) return nullptr;
                if (end_is_set) return fail(CompileError::BadClass, item_pos, "the end of a range cannot be a class like \\d");
                if (hi < lo) return fail(CompileError::BadClass, item_pos, "the range is out of order");
                for (unsigned c = lo; c <= hi; ++c) bits.set(c);
            } else {
                bits.set(lo);
            }
        }
        if (items == 0) return fail(CompileError::BadClass, open, "an empty class matches nothing");
        if (negate) bits.flip();
        return make_set(bits);
    }
};

}  // namespace detail

inline ParseResult parse(std::string_view pattern) {
    detail::Parser parser(pattern);
    return parser.run();
}

}  // namespace rx
