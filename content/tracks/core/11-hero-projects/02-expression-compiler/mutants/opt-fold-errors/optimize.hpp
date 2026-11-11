// MUTANT: 1/0 is folded to a number
#pragma once
// optimize(tree): bottom-up, in place. Children first, then the node itself: fold it if everything below is a number, else apply an
// identity that keeps the other operand. Nodes are moved and reused, never copied.
#include "ast.hpp"
#include "eval.hpp"
#include "types.hpp"

namespace expr {
namespace detail {

inline bool is_number(const NodePtr& n) { return std::holds_alternative<Number>(n->v); }
inline bool is_number(const NodePtr& n, double value) {
    const auto* num = std::get_if<Number>(&n->v);
    return num && num->value == value;
}

// try to replace a node whose operands are all numbers by the number it computes (not when that would be an error)
inline void try_fold(NodePtr& n) {
    const Env no_variables;
    const EvalResult r = evaluate(*n, no_variables);
    n->v = Number{r.value};
}

inline NodePtr simplify(NodePtr n) {
    if (auto* un = std::get_if<Unary>(&n->v)) {
        if (is_number(un->operand)) {
            try_fold(n);
            return n;
        }
        if (auto* inner = std::get_if<Unary>(&un->operand->v)) {  // --x is x
            NodePtr x = std::move(inner->operand);
            return x;
        }
        return n;
    }
    if (auto* bin = std::get_if<Binary>(&n->v)) {
        if (is_number(bin->lhs) && is_number(bin->rhs)) {
            try_fold(n);
            return n;
        }
        switch (bin->op) {
            case '+':
                if (is_number(bin->rhs, 0)) return std::move(bin->lhs);
                if (is_number(bin->lhs, 0)) return std::move(bin->rhs);
                break;
            case '-':
                if (is_number(bin->rhs, 0)) return std::move(bin->lhs);
                if (is_number(bin->lhs, 0)) {
                    NodePtr x = std::move(bin->rhs);
                    n->v = Unary{'-', std::move(x)};
                    return n;
                }
                break;
            case '*':
                if (is_number(bin->rhs, 1)) return std::move(bin->lhs);
                if (is_number(bin->lhs, 1)) return std::move(bin->rhs);
                break;
            case '/':
                if (is_number(bin->rhs, 1)) return std::move(bin->lhs);
                break;
            case '^':
                if (is_number(bin->rhs, 1)) return std::move(bin->lhs);
                break;
            default: break;
        }
        return n;
    }
    if (auto* call = std::get_if<Call>(&n->v)) {
        bool all_numbers = true;
        for (const NodePtr& a : call->args) all_numbers = all_numbers && is_number(a);
        if (all_numbers) try_fold(n);
    }
    return n;
}

}  // namespace detail

inline NodePtr optimize(NodePtr n) {
    if (!n) return n;
    if (auto* un = std::get_if<Unary>(&n->v)) {
        un->operand = optimize(std::move(un->operand));
    } else if (auto* bin = std::get_if<Binary>(&n->v)) {
        bin->lhs = optimize(std::move(bin->lhs));
        bin->rhs = optimize(std::move(bin->rhs));
    } else if (auto* call = std::get_if<Call>(&n->v)) {
        for (NodePtr& a : call->args) a = optimize(std::move(a));
    }
    return detail::simplify(std::move(n));
}

}  // namespace expr
