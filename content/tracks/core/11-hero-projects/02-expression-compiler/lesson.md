# Hero Project: Expression Compiler

> **From text to a number, in four stages.** Every calculator, spreadsheet, query language and compiler does the same thing: *tokenize* the characters, *parse* the tokens into a tree, *evaluate* (or generate code from) the tree, and *optimize* the tree on the way. You will write all four stages, for an arithmetic language with variables and functions.

## The language

```
2 * (3 + 4) - x ^ 2          -x^2 means -(x^2)         max(1, y % 3)
```

- Numbers (`12`, `3.5`, `1e-3`), names (`x`, `rate_2`), the operators `+ - * / % ^`, parentheses and commas. All values are `double`.
- Precedence, lowest to highest: `+ -`, then `* / %`, then prefix `-` / `+`, then `^`. `+ - * / %` are **left**-associative (`10-4-3` is `(10-4)-3`); `^` is **right**-associative (`2^3^2` is `2^(3^2)`), and `-2^2` is `-(2^2)`, but the exponent may have its own sign (`2^-1`).
- Functions: `sqrt abs floor ceil` (one argument), `min max pow` (two). A name followed by `(` must be a function; a name without `(` is a variable.
- `%` is `std::fmod` (the sign follows the left side: `-7 % 3` is `-1`), `^` is `std::pow`.

## The four stages (four files)

| File | Function | What it does |
|---|---|---|
| `lexer.hpp` | `tokenize(text)` | Characters → `Token`s (`kind`, `number`/`text`, `pos`), ending with an `End` token. Errors: `BadCharacter`, `BadNumber`. |
| `parser.hpp` | `parse(tokens)` | Tokens → tree. One function per precedence level; errors `UnexpectedToken`, `UnexpectedEnd`, `UnknownFunction`, `WrongArity`, `TooDeep`. |
| `eval.hpp` | `evaluate(node, env)` | Tree → number, or the **first** `EvalError` (left to right). |
| `optimize.hpp` | `optimize(tree)` | Tree → smaller tree with the **same meaning**. |

Read `types.hpp` (tokens, errors, `Env`, counters) and `ast.hpp` (the tree: a `std::variant` of `Number`, `Variable`, `Unary`, `Binary`, `Call`, owned through `std::unique_ptr`, and `to_string` which prints the tree fully parenthesized) first. Both are read-only.

### Errors and positions

Offsets are 0-based bytes. The lexer: `BadCharacter` at the character; `BadNumber` where a digit was needed (`1.` → offset 2; `1e+` → 3), or at the start of a number too large for a double. The parser reports the token that went wrong: `UnexpectedToken` at that token, `UnexpectedEnd` at the `End` token (the length of the text) when more was needed, `UnknownFunction` and `WrongArity` at the **name**. A name is checked as soon as `(` follows it, before its arguments. Nesting through parentheses, prefix operators, `^` chains and call arguments is limited to `kMaxDepth` (200): beyond that `TooDeep`, so `((((…` a hundred thousand times cannot overflow the stack. (A long chain of `+` is a loop, not recursion, and is not limited.)

Evaluation errors: `UnknownVariable` (`detail` is the name), `DivisionByZero` (`/` or `%` by zero), `NotFinite` (any result that is infinity or NaN: `sqrt(-1)`, `pow(0,-1)`, `2^2000`), `BadCall` for a hand-built tree with a wrong call.

### The optimizer's contract

*The same result for every environment, including the same errors.* That rules out some tempting rewrites:

- **Constant folding**: an operation whose operands are all numbers becomes the number it computes. **But** if computing it is an error (`1/0`, `sqrt(-1)`, `2^2000`), leave it alone: the error must still happen at evaluation time.
- **Identities that keep the other side**: `x+0`, `0+x`, `x-0`, `x*1`, `1*x`, `x/1`, `x^1` → `x`; `0-x` → `-x`; `--x` → `x`.
- **Not allowed**: `x*0` → `0` (if `x` is an unknown variable, or `1/0`, the original reports an error and the rewrite would not), `(x+1)+2` → `x+3` (floating-point addition is not associative), `x-x` → `0`.

## The tiers

- **Works**: tokens, precedence and associativity, unary minus, functions, evaluation, folding and identities.
- **Robust**: every error kind with its exact offset, the depth limit, error order in evaluation, errors that survive optimization, and 500 random expressions that must evaluate identically before and after `optimize`.
- **Fast**: 100000 tokens in time (nothing quadratic), bounded allocations in the tokenizer and parser, **no allocation and one `Env` lookup per variable** when evaluating, and an optimizer that rewrites the tree **in place** (50 allocations for 50000 nodes: reuse nodes, do not rebuild).
- **Clean**: no raw `new`/`delete`, no exceptions, no throwing conversions or streams, no `dynamic_cast`, no output.

## How to approach it

Do the stages in order and test each with the Run button (`main.cpp` prints every stage for the lines you type). In the parser, `parse_add` / `parse_mul` loop over their operator; `parse_unary` and the exponent recurse. Return a null pointer on error and remember the error in a member, so that every caller only has to check for null.
