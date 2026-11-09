#pragma once

#include <memory>
#include <string>
#include <utility>
#include <variant>

// The types of a tiny expression language (read-only). An expression is ONE of four kinds of node, which is exactly what std::variant says:
//
//   Num   a number                      3.5
//   Var   a named variable              x
//   BinOp an operator and two operands  (x + 3.5)      op is one of '+', '-', '*', '/'
//   Neg   a negation                    (-x)
//
// A BinOp owns its operands through unique_ptr<Expr>: the tree is recursive, and a variant cannot contain itself directly.

struct Expr;
using ExprPtr = std::unique_ptr<Expr>;

struct Num {
    double value = 0.0;
};
struct Var {
    std::string name;
};
struct BinOp {
    char op = '+';
    ExprPtr lhs;
    ExprPtr rhs;
};
struct Neg {
    ExprPtr operand;
};

struct Expr {
    std::variant<Num, Var, BinOp, Neg> node;
};

// ---- builders: ExprPtr e = bin('+', var("x"), num(3)); ----
inline ExprPtr num(double value) {
    auto e = std::make_unique<Expr>();
    e->node = Num{value};
    return e;
}
inline ExprPtr var(std::string name) {
    auto e = std::make_unique<Expr>();
    e->node = Var{std::move(name)};
    return e;
}
inline ExprPtr bin(char op, ExprPtr lhs, ExprPtr rhs) {
    auto e = std::make_unique<Expr>();
    e->node = BinOp{op, std::move(lhs), std::move(rhs)};
    return e;
}
inline ExprPtr neg(ExprPtr operand) {
    auto e = std::make_unique<Expr>();
    e->node = Neg{std::move(operand)};
    return e;
}

// ---- the "overloaded" helper: combine several lambdas into ONE visitor for std::visit ----
//   std::visit(Overloaded{[](const Num& n) { ... }, [](const Var& v) { ... }}, expr.node);
template <class... Ts>
struct Overloaded : Ts... {
    using Ts::operator()...;
};
template <class... Ts>
Overloaded(Ts...) -> Overloaded<Ts...>;
