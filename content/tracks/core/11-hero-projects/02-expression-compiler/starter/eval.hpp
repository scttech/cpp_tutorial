#pragma once
// YOUR EVALUATOR. evaluate(node, env) computes the value of a tree, or says what went wrong (the FIRST error, evaluating left to right).
//   + - * /   % is std::fmod        ^ is std::pow
//   a / 0 and a % 0 are DivisionByZero; a variable that is not in the Env is UnknownVariable (detail = its name);
//   any result that is not a finite number is NotFinite; a call with an unknown name or the wrong arity is BadCall (detail = the name).
//   Functions: sqrt abs floor ceil (1 argument), min max pow (2 arguments).
#include <cmath>

#include "ast.hpp"
#include "types.hpp"

namespace expr {

inline EvalResult evaluate(const Node& n, const Env& env) {
    (void)env;
    EvalResult r;
    if (const auto* num = std::get_if<Number>(&n.v)) {
        r.value = num->value;
        return r;
    }
    r.error = EvalError::BadCall;  // TODO: variables, unary minus, binary operators, calls
    return r;
}

}  // namespace expr
