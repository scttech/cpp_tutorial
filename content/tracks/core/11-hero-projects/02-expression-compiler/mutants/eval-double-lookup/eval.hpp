// MUTANT: the variable is looked up twice
#pragma once
// evaluate(node, env): a recursive walk over the tree. Nothing is copied: the tree and the Env are only looked at.
#include <cmath>
#include <string>

#include "ast.hpp"
#include "types.hpp"

namespace expr {
namespace detail {

class Evaluator {
public:
    explicit Evaluator(const Env& env) : env_(env) {}

    EvalResult run(const Node& n) {
        EvalResult r;
        if (eval(n, r.value)) return r;
        r.value = 0;
        r.error = error_;
        r.detail = std::move(detail_);
        return r;
    }

private:
    const Env& env_;
    EvalError error_ = EvalError::None;
    std::string detail_;

    bool fail(EvalError e, std::string_view detail = {}) {
        error_ = e;
        detail_ = std::string(detail);
        return false;
    }

    bool eval(const Node& n, double& out) {
        if (const auto* num = std::get_if<Number>(&n.v)) {
            out = num->value;
            return true;
        }
        if (const auto* var = std::get_if<Variable>(&n.v)) {
            (void)env_.find(var->name);
            const double* value = env_.find(var->name);
            if (!value) return fail(EvalError::UnknownVariable, var->name);
            out = *value;
            return true;
        }
        if (const auto* un = std::get_if<Unary>(&n.v)) {
            double x = 0;
            if (!eval(*un->operand, x)) return false;
            out = -x;
            return true;
        }
        if (const auto* bin = std::get_if<Binary>(&n.v)) {
            double a = 0, b = 0;
            if (!eval(*bin->lhs, a)) return false;
            if (!eval(*bin->rhs, b)) return false;
            switch (bin->op) {
                case '+': out = a + b; break;
                case '-': out = a - b; break;
                case '*': out = a * b; break;
                case '/':
                    if (b == 0) return fail(EvalError::DivisionByZero);
                    out = a / b;
                    break;
                case '%':
                    if (b == 0) return fail(EvalError::DivisionByZero);
                    out = std::fmod(a, b);
                    break;
                case '^': out = std::pow(a, b); break;
                default: return fail(EvalError::BadCall, std::string(1, bin->op));
            }
            if (!std::isfinite(out)) return fail(EvalError::NotFinite);
            return true;
        }
        const auto& call = std::get<Call>(n.v);
        const int arity = arity_of(call.name);
        if (arity < 0 || static_cast<std::size_t>(arity) != call.args.size()) return fail(EvalError::BadCall, call.name);
        double a[2] = {0, 0};
        for (std::size_t k = 0; k < call.args.size(); ++k) {
            if (!eval(*call.args[k], a[k])) return false;
        }
        if (call.name == "sqrt") out = std::sqrt(a[0]);
        else if (call.name == "abs") out = std::fabs(a[0]);
        else if (call.name == "floor") out = std::floor(a[0]);
        else if (call.name == "ceil") out = std::ceil(a[0]);
        else if (call.name == "min") out = a[0] < a[1] ? a[0] : a[1];
        else if (call.name == "max") out = a[0] > a[1] ? a[0] : a[1];
        else out = std::pow(a[0], a[1]);
        if (!std::isfinite(out)) return fail(EvalError::NotFinite);
        return true;
    }
};

}  // namespace detail

inline EvalResult evaluate(const Node& n, const Env& env) {
    detail::Evaluator evaluator(env);
    return evaluator.run(n);
}

}  // namespace expr
