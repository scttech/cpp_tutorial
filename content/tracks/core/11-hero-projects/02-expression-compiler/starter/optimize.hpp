#pragma once
// YOUR OPTIMIZER. optimize(tree) returns an equivalent tree that is cheaper to evaluate. It must give the SAME result as the original,
// including the same errors, for every environment. Rewrite the tree in place: moving nodes around is free, building a copy is not.
//   - constant folding: an operation whose operands are all numbers becomes the number it evaluates to (unless that is an error)
//   - identities that keep the other operand: x+0  0+x  x-0  x*1  1*x  x/1  x^1  -> x      0-x -> -x      --x -> x
#include "ast.hpp"
#include "eval.hpp"
#include "types.hpp"

namespace expr {

inline NodePtr optimize(NodePtr n) {
    return n;  // TODO
}

}  // namespace expr
