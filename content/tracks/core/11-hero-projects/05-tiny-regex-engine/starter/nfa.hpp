#pragma once
// YOUR NFA BUILDER. build_nfa(tree) turns the syntax tree into an Nfa (Thompson's construction).
//
// A convenient way: a recursive function  std::size_t compile(const Node&, std::size_t next)  that adds the states for a node and returns
// the state to ENTER it; `next` is the state to continue with after the node matched. Building backwards like this needs no patching:
//   Literal c   -> one Char state, out = next                       Empty   -> just `next`
//   Concat      -> compile the parts from the last to the first, each one continuing with the entry of the one after it
//   Alt         -> compile every option with the same `next`, join their entries with Split states
//   x*          -> a Split whose out is the entry of x (x continues with that Split again) and whose out1 is `next`
//   x+          -> like x*, but the entry is the entry of x;     x?  -> a Split with out = entry of x, out1 = next
// The whole NFA ends in one Match state; nfa.set_start(entry of the tree).
#include "types.hpp"

namespace rx {

inline Nfa build_nfa(const Node& root) {
    (void)root;
    Nfa nfa;
    State match;
    match.op = Op::Match;
    nfa.set_start(nfa.add(match));  // TODO: a regex that matches only the empty string
    return nfa;
}

}  // namespace rx
