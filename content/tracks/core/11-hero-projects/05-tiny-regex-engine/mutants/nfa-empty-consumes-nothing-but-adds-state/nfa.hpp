// MUTANT: Empty is a Jump state
#pragma once
// build_nfa(tree): Thompson's construction, built backwards: compile(node, next) adds the states of `node` and returns the state
// that enters it; `next` is where to continue after the node has matched. No patching of dangling arrows is needed.
#include <variant>

#include "types.hpp"

namespace rx {
namespace detail {

inline std::size_t simple(Nfa& nfa, Op op, unsigned char ch, std::size_t set, std::size_t next) {
    State s;
    s.op = op;
    s.ch = ch;
    s.set = set;
    s.out = next;
    return nfa.add(s);
}

inline std::size_t compile(Nfa& nfa, const Node& node, std::size_t next) {
    if (std::holds_alternative<Empty>(node.v)) return simple(nfa, Op::Jump, 0, 0, next);
    if (const auto* lit = std::get_if<Literal>(&node.v)) return simple(nfa, Op::Char, lit->ch, 0, next);
    if (std::holds_alternative<AnyChar>(node.v)) return simple(nfa, Op::Any, 0, 0, next);
    if (const auto* set = std::get_if<CharSet>(&node.v)) return simple(nfa, Op::Set, 0, nfa.add_set(set->bits), next);
    if (std::holds_alternative<LineStart>(node.v)) return simple(nfa, Op::Begin, 0, 0, next);
    if (std::holds_alternative<LineEnd>(node.v)) return simple(nfa, Op::End, 0, 0, next);
    if (const auto* cat = std::get_if<Concat>(&node.v)) {
        std::size_t entry = next;
        for (std::size_t i = cat->parts.size(); i-- > 0;) entry = compile(nfa, *cat->parts[i], entry);
        return entry;
    }
    if (const auto* alt = std::get_if<Alt>(&node.v)) {
        std::size_t entry = compile(nfa, *alt->options.back(), next);
        for (std::size_t i = alt->options.size() - 1; i-- > 0;) {
            const std::size_t option = compile(nfa, *alt->options[i], next);
            State split;
            split.op = Op::Split;
            split.out = option;
            split.out1 = entry;
            entry = nfa.add(split);
        }
        return entry;
    }
    const auto& rep = std::get<Repeat>(node.v);
    State split;
    split.op = Op::Split;
    if (rep.kind == '?') {
        split.out = compile(nfa, *rep.child, next);
        split.out1 = next;
        return nfa.add(split);
    }
    split.out1 = next;
    const std::size_t loop = nfa.add(split);  // its out is patched when the body is known: the body continues with the loop again
    const std::size_t body = compile(nfa, *rep.child, loop);
    nfa.edit(loop).out = body;
    return rep.kind == '*' ? loop : body;
}

}  // namespace detail

inline Nfa build_nfa(const Node& root) {
    Nfa nfa;
    State match;
    match.op = Op::Match;
    const std::size_t accept = nfa.add(match);
    nfa.set_start(detail::compile(nfa, root, accept));
    return nfa;
}

}  // namespace rx
