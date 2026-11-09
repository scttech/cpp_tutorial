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

// Every function is a stub that compiles but is wrong: replace each body. expr_types.hpp (read-only) defines the nodes (Num, Var, BinOp, Neg),
// the builders (num, var, bin, neg) and the `Overloaded` helper.

// ---- std::variant + std::visit: one function per KIND of node, and the compiler checks that you covered them all ------------------------
// TODO: evaluate the expression. `vars` gives the values of the variables. The result is std::optional<double>: an EMPTY optional means
// "no value": a variable that is not in `vars`, or a division by zero (in any sub-expression).
//   return std::visit(Overloaded{ [](const Num& n) -> std::optional<double> { ... }, [&](const Var& v) -> ... , ... }, e.node);
// Each lambda handles one alternative; leaving one out is a compile error. Recurse with evaluate(*b.lhs, vars).
inline std::optional<double> evaluate(const Expr& e, const std::map<std::string, double>& vars) {
    (void)e;
    (void)vars;
    return std::nullopt;
}

// TODO: fully parenthesized infix text: a number as written by `os << value` (std::ostringstream), a variable by its name, "(l op r)" for a
// BinOp (a space around the operator), "(-x)" for a Neg.   to_string(bin('+', var("x"), num(3))) is "(x + 3)"
inline std::string to_string(const Expr& e) {
    (void)e;
    return "";
}

// TODO: the number of nodes in the tree (a lone number is 1). Must not allocate.
inline std::size_t node_count(const Expr& e) {
    (void)e;
    return 0;
}

// TODO: the names of the variables used, each once, sorted A to Z. (A helper that fills a std::set<std::string> is the easy way.)
inline std::vector<std::string> variables(const Expr& e) {
    (void)e;
    return {};
}

// TODO: a deep copy: a new tree with no node in common with the original.   num(n.value), var(v.name), bin(b.op, clone(*b.lhs), clone(*b.rhs)), ...
inline ExprPtr clone(const Expr& e) {
    (void)e;
    return num(0);
}

// TODO: constant folding: returns a NEW tree in which every operation whose operands are both numbers (after simplifying them) is replaced by its
// result, and a negated number by the negative number. A division by zero is left as it is. The original is not changed.
//   simplify((2 + 3) * x) is (5 * x)       std::get_if<Num>(&e->node) is a pointer to the Num, or nullptr if the node is something else
inline ExprPtr simplify(const Expr& e) {
    (void)e;
    return num(0);
}

// ---- optional, pair, span, structured bindings ---------------------------------------------------------------------------------------------
// TODO: the smallest and the largest value, or nothing for an empty sequence:  auto mm = min_max(v); if (mm) { auto [lo, hi] = *mm; }
// std::span<const double> is a (pointer, length) view: a vector, an array or a C array all convert to it, with no copy. (std::minmax_element)
inline std::optional<std::pair<double, double>> min_max(std::span<const double> values) {
    (void)values;
    return std::nullopt;
}

// TODO: split off the first element: the head, and a span of everything after it (values.subspan(1), no copy). Nothing for an empty sequence.
inline std::optional<std::pair<int, std::span<const int>>> head_tail(std::span<const int> values) {
    (void)values;
    return std::nullopt;
}
