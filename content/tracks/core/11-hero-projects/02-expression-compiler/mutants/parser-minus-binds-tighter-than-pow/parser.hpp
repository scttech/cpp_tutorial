// MUTANT: -2^2 is (-2)^2
#pragma once
// parse(tokens): recursive descent, one function per precedence level. Binary operators of one level are a loop (left-associative);
// ^ and prefix minus recurse (right-associative). The depth of that recursion is limited, so hostile input cannot overflow the stack.
#include <string>
#include <vector>

#include "ast.hpp"
#include "types.hpp"

namespace expr {
namespace detail {

class Parser {
public:
    explicit Parser(const std::vector<Token>& tokens) : t_(tokens) {}

    ParseResult run() {
        ParseResult r;
        NodePtr e = parse_add(0);
        if (e && peek().kind != Kind::End) {
            unexpected("expected an operator or the end of the expression");
            e = nullptr;
        }
        if (e) {
            r.ast = std::move(e);
        } else {
            r.error = err_;
            r.pos = pos_;
            r.message = msg_;
        }
        return r;
    }

private:
    const std::vector<Token>& t_;
    std::size_t i_ = 0;
    Error err_ = Error::None;
    std::size_t pos_ = 0;
    std::string msg_;

    const Token& peek() const { return t_[i_ < t_.size() ? i_ : t_.size() - 1]; }
    void advance() {
        if (i_ + 1 < t_.size()) ++i_;
    }
    NodePtr fail(Error e, std::size_t pos, std::string message) {
        err_ = e;
        pos_ = pos;
        msg_ = std::move(message);
        return nullptr;
    }
    // the current token cannot be here: the end of the input is a different error from a wrong token
    NodePtr unexpected(std::string message) {
        const Token& t = peek();
        return fail(t.kind == Kind::End ? Error::UnexpectedEnd : Error::UnexpectedToken, t.pos, std::move(message));
    }

    NodePtr parse_add(int depth) {
        NodePtr lhs = parse_mul(depth);
        while (lhs && (peek().kind == Kind::Plus || peek().kind == Kind::Minus)) {
            const char op = peek().kind == Kind::Plus ? '+' : '-';
            advance();
            NodePtr rhs = parse_mul(depth);
            if (!rhs) return nullptr;
            lhs = make_binary(op, std::move(lhs), std::move(rhs));
        }
        return lhs;
    }

    NodePtr parse_mul(int depth) {
        NodePtr lhs = parse_unary(depth);
        while (lhs && (peek().kind == Kind::Star || peek().kind == Kind::Slash || peek().kind == Kind::Percent)) {
            const char op = peek().kind == Kind::Star ? '*' : peek().kind == Kind::Slash ? '/' : '%';
            advance();
            NodePtr rhs = parse_unary(depth);
            if (!rhs) return nullptr;
            lhs = make_binary(op, std::move(lhs), std::move(rhs));
        }
        return lhs;
    }

    NodePtr parse_unary(int depth) {
        if (depth > kMaxDepth) return fail(Error::TooDeep, peek().pos, "the expression is nested too deeply");
        if (peek().kind == Kind::Minus) {
            advance();
            NodePtr operand = parse_primary(depth + 1);
            if (!operand) return nullptr;
            NodePtr negated = make_unary('-', std::move(operand));
            if (peek().kind == Kind::Caret) {
                advance();
                NodePtr exponent = parse_unary(depth + 1);
                if (!exponent) return nullptr;
                return make_binary('^', std::move(negated), std::move(exponent));
            }
            return negated;
        }
        if (peek().kind == Kind::Plus) {
            advance();
            return parse_unary(depth + 1);
        }
        NodePtr base = parse_primary(depth);
        if (!base) return nullptr;
        if (peek().kind == Kind::Caret) {
            advance();
            NodePtr exponent = parse_unary(depth + 1);
            if (!exponent) return nullptr;
            return make_binary('^', std::move(base), std::move(exponent));
        }
        return base;
    }

    NodePtr parse_pow(int depth) {
        NodePtr base = parse_primary(depth);
        if (!base) return nullptr;
        if (peek().kind == Kind::Caret) {
            advance();
            NodePtr exponent = parse_unary(depth + 1);
            if (!exponent) return nullptr;
            return make_binary('^', std::move(base), std::move(exponent));
        }
        return base;
    }

    NodePtr parse_primary(int depth) {
        const Token& t = peek();
        if (t.kind == Kind::Number) {
            const double value = t.number;
            advance();
            return make_number(value);
        }
        if (t.kind == Kind::Ident) {
            const std::size_t name_pos = t.pos;
            std::string name = t.text;
            advance();
            if (peek().kind != Kind::LParen) return make_variable(std::move(name));
            if (arity_of(name) < 0) return fail(Error::UnknownFunction, name_pos, "there is no function called " + name);
            advance();  // the (
            std::vector<NodePtr> args;
            if (peek().kind != Kind::RParen) {
                while (true) {
                    NodePtr arg = parse_add(depth + 1);
                    if (!arg) return nullptr;
                    args.push_back(std::move(arg));
                    if (peek().kind == Kind::Comma) {
                        advance();
                        continue;
                    }
                    break;
                }
            }
            if (peek().kind != Kind::RParen) return unexpected("expected ',' or ')'");
            advance();
            if (static_cast<int>(args.size()) != arity_of(name)) return fail(Error::WrongArity, name_pos, name + " takes " + std::to_string(arity_of(name)) + " argument(s)");
            return make_call(std::move(name), std::move(args));
        }
        if (t.kind == Kind::LParen) {
            advance();
            NodePtr inner = parse_add(depth + 1);
            if (!inner) return nullptr;
            if (peek().kind != Kind::RParen) return unexpected("expected ')'");
            advance();
            return inner;
        }
        return unexpected("expected a number, a name or '('");
    }
};

}  // namespace detail

inline ParseResult parse(const std::vector<Token>& tokens) {
    if (tokens.empty()) {
        ParseResult r;
        r.error = Error::UnexpectedEnd;
        r.message = "nothing to parse";
        return r;
    }
    detail::Parser parser(tokens);
    return parser.run();
}

}  // namespace expr
