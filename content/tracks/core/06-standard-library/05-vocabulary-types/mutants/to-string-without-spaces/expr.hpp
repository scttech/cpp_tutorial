// MUTANT: (x+3).
#pragma once

#include <algorithm>
#include <cstddef>
#include <map>
#include <optional>
#include <set>
#include <span>
#include <sstream>
#include <string>
#include <utility>
#include <variant>
#include <vector>

#include "expr_types.hpp"

// ---- std::variant + std::visit: one function per KIND of node, and the compiler checks that you covered them all ------------------------
// Evaluates the expression. `vars` gives the values of the variables. The result is std::optional<double>: an EMPTY optional means
// "no value": a variable that is not in `vars`, or a division by zero (in any sub-expression).
inline std::optional<double> evaluate(const Expr& e, const std::map<std::string, double>& vars) {
    return std::visit(
        Overloaded{
            [](const Num& n) -> std::optional<double> { return n.value; },
            [&](const Var& v) -> std::optional<double> {
                const auto it = vars.find(v.name);
                if (it == vars.end()) return std::nullopt;
                return it->second;
            },
            [&](const BinOp& b) -> std::optional<double> {
                const std::optional<double> l = evaluate(*b.lhs, vars);
                const std::optional<double> r = evaluate(*b.rhs, vars);
                if (!l || !r) return std::nullopt;
                switch (b.op) {
                    case '+': return *l + *r;
                    case '-': return *l - *r;
                    case '*': return *l * *r;
                    case '/': return *r == 0.0 ? std::nullopt : std::optional<double>(*l / *r);
                    default: return std::nullopt;
                }
            },
            [&](const Neg& n) -> std::optional<double> {
                const std::optional<double> v = evaluate(*n.operand, vars);
                if (!v) return std::nullopt;
                return -*v;
            },
        },
        e.node);
}

// Fully parenthesized infix text: a number as written by `os << value`, a variable by its name, "(l op r)" for a BinOp (a space around the
// operator), "(-x)" for a Neg.   to_string(bin('+', var("x"), num(3))) is "(x + 3)"
inline std::string to_string(const Expr& e) {
    return std::visit(
        Overloaded{
            [](const Num& n) {
                std::ostringstream os;
                os << n.value;
                return os.str();
            },
            [](const Var& v) { return v.name; },
            [](const BinOp& b) { return "(" + to_string(*b.lhs) + std::string(1, b.op) + to_string(*b.rhs) + ")"; },
            [](const Neg& n) { return "(-" + to_string(*n.operand) + ")"; },
        },
        e.node);
}

// The number of nodes in the tree (a lone number is 1). Must not allocate.
inline std::size_t node_count(const Expr& e) {
    return std::visit(
        Overloaded{
            [](const Num&) -> std::size_t { return 1; },
            [](const Var&) -> std::size_t { return 1; },
            [](const BinOp& b) -> std::size_t { return 1 + node_count(*b.lhs) + node_count(*b.rhs); },
            [](const Neg& n) -> std::size_t { return 1 + node_count(*n.operand); },
        },
        e.node);
}

namespace detail {
inline void collect_variables(const Expr& e, std::set<std::string>& out) {
    std::visit(
        Overloaded{
            [](const Num&) {},
            [&](const Var& v) { out.insert(v.name); },
            [&](const BinOp& b) {
                collect_variables(*b.lhs, out);
                collect_variables(*b.rhs, out);
            },
            [&](const Neg& n) { collect_variables(*n.operand, out); },
        },
        e.node);
}
}  // namespace detail

// The names of the variables used, each once, sorted A to Z.
inline std::vector<std::string> variables(const Expr& e) {
    std::set<std::string> names;
    detail::collect_variables(e, names);
    return std::vector<std::string>(names.begin(), names.end());
}

// A deep copy: a new tree with no node in common with the original.
inline ExprPtr clone(const Expr& e) {
    return std::visit(
        Overloaded{
            [](const Num& n) { return num(n.value); },
            [](const Var& v) { return var(v.name); },
            [](const BinOp& b) { return bin(b.op, clone(*b.lhs), clone(*b.rhs)); },
            [](const Neg& n) { return neg(clone(*n.operand)); },
        },
        e.node);
}

// Constant folding: returns a NEW tree in which every operation whose operands are both numbers (after simplifying them) is replaced by its
// result, and a negated number by the negative number. A division by zero is left as it is. The original is not changed.
//   simplify((2 + 3) * x) is (5 * x)
inline ExprPtr simplify(const Expr& e) {
    return std::visit(
        Overloaded{
            [](const Num& n) { return num(n.value); },
            [](const Var& v) { return var(v.name); },
            [](const BinOp& b) -> ExprPtr {
                ExprPtr l = simplify(*b.lhs);
                ExprPtr r = simplify(*b.rhs);
                const Num* ln = std::get_if<Num>(&l->node);
                const Num* rn = std::get_if<Num>(&r->node);
                if (ln && rn) {
                    switch (b.op) {
                        case '+': return num(ln->value + rn->value);
                        case '-': return num(ln->value - rn->value);
                        case '*': return num(ln->value * rn->value);
                        case '/':
                            if (rn->value != 0.0) return num(ln->value / rn->value);
                            break;
                        default: break;
                    }
                }
                return bin(b.op, std::move(l), std::move(r));
            },
            [](const Neg& n) -> ExprPtr {
                ExprPtr inner = simplify(*n.operand);
                if (const Num* v = std::get_if<Num>(&inner->node)) return num(-v->value);
                return neg(std::move(inner));
            },
        },
        e.node);
}

// ---- optional, pair, span, structured bindings ---------------------------------------------------------------------------------------------
// The smallest and the largest value, or nothing for an empty sequence:  auto mm = min_max(v); if (mm) { auto [lo, hi] = *mm; }
// std::span<const double> is a (pointer, length) view: a vector, an array or a C array all convert to it, with no copy.
inline std::optional<std::pair<double, double>> min_max(std::span<const double> values) {
    if (values.empty()) return std::nullopt;
    const auto [lo, hi] = std::minmax_element(values.begin(), values.end());
    return std::make_pair(*lo, *hi);
}

// Splits off the first element: the head, and a span of everything after it (no copy). Nothing for an empty sequence.
inline std::optional<std::pair<int, std::span<const int>>> head_tail(std::span<const int> values) {
    if (values.empty()) return std::nullopt;
    return std::make_pair(values.front(), values.subspan(1));
}
