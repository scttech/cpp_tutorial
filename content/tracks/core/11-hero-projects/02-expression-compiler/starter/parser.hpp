#pragma once
// YOUR PARSER. parse(tokens) builds the tree (see ast.hpp) or reports the first error with the offset of the token that went wrong.
//   expr    = add
//   add     = mul { ('+' | '-') mul }              left-associative
//   mul     = unary { ('*' | '/' | '%') unary }    left-associative
//   unary   = '-' unary | '+' unary | pow
//   pow     = primary [ '^' unary ]                right-associative; binds tighter than a minus in front: -2^2 is -(2^2)
//   primary = number | name | name '(' [ expr { ',' expr } ] ')' | '(' expr ')'
#include <string>
#include <vector>

#include "ast.hpp"
#include "types.hpp"

namespace expr {

inline ParseResult parse(const std::vector<Token>& tokens) {
    ParseResult r;
    // TODO: a recursive-descent parser. This skeleton only accepts a single number.
    if (tokens.size() == 2 && tokens[0].kind == Kind::Number) {
        r.ast = make_number(tokens[0].number);
        return r;
    }
    r.error = Error::UnexpectedToken;
    r.pos = tokens.empty() ? 0 : tokens[0].pos;
    r.message = "not implemented yet";
    return r;
}

}  // namespace expr
