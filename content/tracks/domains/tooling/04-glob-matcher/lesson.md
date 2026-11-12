# A Glob Matcher That Cannot Be Made to Backtrack

> **You're on the developer-tools team.** `.gitignore`, build systems, test selectors, CI path filters, `rsync --exclude`, editors' "files to include" boxes and `.dockerignore` all use *globs*: `*.cpp`, `src/**/test_?.hpp`, `{a,b}`. Users type patterns you do not control, and the paths come from a file system you do not control. A matcher that takes exponential time on `*a*a*a*a*a*a*b` is a denial-of-service waiting for a monorepo with a long file name; a matcher that lets `*` cross a `/` leaks files into the wrong rule. You will write one with the semantics people expect, and a running time you can bound.

## The semantics, which are what people actually mean

- `*` and `?` and `[...]` stay inside one path segment: they never match `/`.
- `**` as a *whole segment* crosses segments: `**/x` finds `x` anywhere (including at the top), `a/**/x` has zero or more directories between, and `a/**` is "everything under `a`".
- A **leading dot** is special: `*` does not match `.git` (that is why `ls *` does not list hidden files). The pattern has to say `.*`. The rule applies to the first character of *every* segment, so `**/x` does not look inside `.cache/` either. A switch (`dot`) turns the rule off.
- Sets: `[abc]`, `[a-f]`, `[!abc]`, with the two traditional oddities: a `]` first in the set is a member, and `-` at either end is a hyphen. Sets never match `/`.
- Braces `{a,b}` are alternatives, may nest, may contain wildcards.
- `\` makes the next character literal.

## Why recursion is the wrong tool

The textbook matcher is recursive: on `*`, try matching the rest of the pattern at every possible position. Two stars multiply the work (`n^2`), ten stars give `n^10`, and a pattern that *almost* matches is the worst case, because every combination has to be tried and rejected. `{a,aa,aaa}` repeated thirty times is worse: `3^30` ways to split the same text.

The fix is the one regular-expression engines like RE2 and Rust's `regex` use (Russ Cox's classic article "Regular Expression Matching Can Be Simple And Fast"): **compile the pattern into a tiny program**, then **simulate all possible positions at once**. At each character keep the *set* of program locations that are still alive. A set has at most `m` members, there are `n` characters, so the whole match is `O(n * m)` for every pattern, with no recursion on the path at all.

The program instructions are almost the whole design:

| Instruction | Meaning |
|---|---|
| `Char c` / `Any` / `Set k` | consume one character, if it fits |
| `Split x, y` | continue at *both* `x` and `y` (alternation, the choice in `*`) |
| `Jmp x` | continue at `x` (the loop back of `*`, the exit of an alternative) |
| `Guard` | only continue if the dot rule allows a wildcard here |
| `Match` | accept, if the path is used up |

`*` compiles to `L: Split(body, out); body: AnyNoSlash; Jmp L; out:`, an alternative `{A,B}` to `Split(A, B)` with both branches jumping to the end, `**/` to a loop of "a segment followed by a slash". Following `Split` and `Jmp` (the **epsilon closure**) uses an explicit stack, so a pattern of a million characters cannot overflow the call stack; and a per-position "already visited" mark makes sure each program location enters the set once.

## Patterns can be wrong

`[abc`, `{a,b`, `[z-a]` and a trailing `\` are errors, and an error needs a **position**: the byte offset of the `[` or `{` that was never closed, or of the first character of the reversed range. An invalid pattern matches nothing (it must not be half-interpreted). Nesting of braces is the one recursive part of the compiler, so it is limited (32): a million `{` must produce an error, not a crash.

## Compile once, match many

`Glob::compile` does the parsing once; `matches` can then be called for every path. Its allocation count must not depend on the path length (the tests check with a 25 KB path).

## The tiers

- **Works**: every syntax element on its own, dotfiles in all forms, case folding, error positions, nesting limit, reuse.
- **Robust**: catastrophic patterns; a 1,000,000-character pattern and path; **4000 random patterns generated as syntax trees and evaluated by a separate interpreter**; garbage; escaping everything gives a literal.
- **Fast**: bounded allocations; 40000 paths against 12 patterns.
- **Clean**: no raw memory or exceptions; no regex, `fnmatch`, filesystem or `std::function`.
