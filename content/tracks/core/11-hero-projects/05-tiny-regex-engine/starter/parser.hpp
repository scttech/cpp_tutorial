#pragma once
// YOUR PARSER. parse(pattern) builds the syntax tree (types.hpp) or reports the first error with its offset.
//
//   alt     = concat { '|' concat }          the lowest precedence; an empty branch is allowed: "a|" "(|a)"
//   concat  = { postfix }                    nothing at all is the empty regex (make_empty)
//   postfix = atom { '*' | '+' | '?' }       quantifiers can be stacked: a** is (* (* a))
//   atom    = literal | '.' | '^' | '$' | '(' alt ')' | '[' class ']' | '\' escape
//
// A group does not make a node of its own: "(a)" is just the tree of a. A concat of one part is that part; an alt of one
// option is that option; otherwise make_concat / make_alt with all the parts (no nesting across groups: (ab)c is (cat (cat a b) c)).
// See the lesson for the escapes, the classes and the errors. The skeleton below only knows literal characters.
#include <string>
#include <string_view>
#include <vector>

#include "types.hpp"

namespace rx {

inline ParseResult parse(std::string_view pattern) {
    ParseResult r;
    std::vector<NodePtr> parts;
    for (const char c : pattern) {
        // TODO: everything else: . ^ $ ( ) [ ] \ | * + ?
        parts.push_back(make_literal(static_cast<unsigned char>(c)));
    }
    if (parts.empty()) r.ast = make_empty();
    else if (parts.size() == 1) r.ast = std::move(parts[0]);
    else r.ast = make_concat(std::move(parts));
    return r;
}

}  // namespace rx
