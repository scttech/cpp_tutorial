# Hero Project: Tiny Regex Engine

> **Regular expressions without backtracking.** Most regex libraries try the alternatives one after another and go back when one fails. That is simple, and on `(a*)*b` against a text of thirty `a` it takes longer than the age of the universe. The theory of *finite automata* gives a matcher that is **linear in the text, for every pattern**: translate the regex into a small state machine (an NFA) and run *all* its possible paths at once. Google's RE2, Rust's `regex` and `grep` work this way. You will build one.

## Three stages, three files

| File | Function | What it does |
|---|---|---|
| `parser.hpp` | `parse(pattern)` | pattern text → syntax tree (`Node` variant: `Literal`, `AnyChar`, `CharSet`, `Concat`, `Alt`, `Repeat`, anchors) |
| `nfa.hpp` | `build_nfa(tree)` | syntax tree → `Nfa` (Thompson's construction): a vector of states with arrows |
| `matcher.hpp` | `nfa_full_match`, `nfa_search` | run the NFA on a text, with a *set* of current states |

`types.hpp` (read-only) has the tree, the `State`/`Nfa` types, the errors; `regex.hpp` (read-only) is the public face (`compile`, `Regex::full_match`, `search`, `find_all`) built on your three functions. **The `Nfa` counts how often you look at a state** (`nfa.at(i)`), which is how the Fast tests prove that your matcher does not backtrack.

## The language

- Atoms: a literal byte; `.` (any byte except `\n`); `[abc]`, `[a-z]`, `[^0-9]` (a negated class also matches `\n`); `\d \w \s` and their opposites `\D \W \S`; `\n \t \r`; `\` + one of `\ . * + ? | ( ) [ ] ^ $` is that literal; `^` start of the text, `$` end of the text (absolute: `search(text, from)` with `from > 0` does not make `^` match at `from`). Everything else, including `]`, `{`, `}`, `-`, is a literal.
- Postfix: `*` (zero or more), `+` (one or more), `?` (optional), stackable: `a**` is `(a*)*`. They cannot follow `^`, `$` or nothing.
- `|` is the lowest precedence, then concatenation, then the postfix operators. Branches may be empty: `a|`, `(|a)b`, `()`; the empty pattern matches the empty string. Parentheses only group (no capturing).
- **Classes**: `\d \w \s` work inside `[...]` too; escapes `\]` `\\` `\-` `\^` and the metacharacters, `\n \t \r`. A `-` is a range operator only between two single characters (`a-z`); at the start or end, or after a range or a class, it is a literal (`[a-]`, `[-a]`, `[a-c-e]`). `]` closes the class even as its first character (`[]` is an error; write `[\]]`). Ranges must be in order.
- Matching works on **bytes** (no UTF-8 awareness; `\xFF` and `\0` are ordinary bytes). `search` finds the **leftmost** match, and among the matches that start there, the **longest**: `a|ab|abc` in `xabcd` is `[1,4)`.

## Errors

`CompileError` with a 0-based offset; the tests check them exactly.

| Error | When | Position |
|---|---|---|
| `UnmatchedParen` | `)` without `(` · a `(` that is never closed | the `)` · that `(` (the innermost unclosed one) |
| `NothingToRepeat` | `*` `+` `?` at the start, after `(` or `\|`, or after `^` `$` | the quantifier |
| `BadEscape` | an unknown escape (`\a`, `\-` outside a class), a trailing `\` | the backslash |
| `BadClass` | `[` never closed · `[]` · `[^]` · a range out of order `[z-a]` · a range ending in a class `[a-\d]` | the `[` · the start of the range |
| `TooDeep` | more than `kMaxDepth` (100) nested parentheses, or 100 stacked quantifiers | the offending `(` (index 100 for the 101st) or quantifier |

## How an NFA run works

The NFA is a graph: `Char`/`Any`/`Set` states consume one byte; `Split` has two exits and consumes nothing; `Begin`/`End` are assertions; `Match` is the goal. After every input byte you hold **the set of states you could be in**. Before looking at the next byte, add everything reachable without consuming input (the *epsilon closure*, with a "seen" mark so that the loops in `(a*)*` cannot spin forever). Each state is processed once per byte, so the cost is (text length) × (number of states).

To search for the leftmost-longest match in **one pass**, remember with each state **where its match started**; a state reached by two starts keeps the earlier one (it will win). When a `Match` state is reached, that start is the answer's start; keep running only to find the longest end, and stop starting new threads.

## The tiers

- **Works**: the syntax tree for every construct, matching, searching, anchors, `find_all`.
- **Robust**: every error with its position, class edge cases, bytes, the leftmost-longest rule, empty matches, patterns that kill backtracking engines, depth limits, 5000-piece patterns, and a **differential test**: 600 random patterns on random texts are compared with an independent evaluation of your own syntax tree.
- **Fast**: at most 2.5 state visits per (byte × state) on 20000-byte texts, no restart at every offset, linear growth, a bounded number of allocations, an NFA with at most one state per pattern character.
- **Clean**: no raw `new`/`delete`, no `<regex>` (you are writing it!), no exceptions, a simulation made of vectors.

## How to approach it

Parser first (the shape tests only need it), then `build_nfa` and `nfa_full_match` (everything in the "full_match" tests), then `nfa_search`: a simple version restarts `nfa_full_match`-like runs at every offset, which passes Works and Robust; the one-pass version with start offsets is what the Fast tests demand.
