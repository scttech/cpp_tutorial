#pragma once
// YOUR MATCHER: run the NFA on a text WITHOUT backtracking. Keep the set of states the NFA can be in right now ("threads"), and for
// every input byte compute the next set. Look at states only through nfa.at(i) (every look is counted).
//
//   nfa_full_match(nfa, text)       does the whole text match?
//   nfa_search(nfa, text, from)     the leftmost match starting at or after `from`; if several start there, the longest.
//                                   ^ matches only at offset 0 of the text, $ only at its end (absolute positions, whatever `from` is).
#include <optional>
#include <string_view>
#include <vector>

#include "types.hpp"

namespace rx {

inline bool nfa_full_match(const Nfa& nfa, std::string_view text) {
    (void)nfa;
    (void)text;
    return false;  // TODO
}

inline std::optional<Match> nfa_search(const Nfa& nfa, std::string_view text, std::size_t from) {
    (void)nfa;
    (void)text;
    (void)from;
    return std::nullopt;  // TODO
}

}  // namespace rx
