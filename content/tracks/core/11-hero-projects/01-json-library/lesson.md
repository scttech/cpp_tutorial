# Hero Project: JSON Library

> **A real format, done properly.** JSON looks tiny, and every page of its grammar hides a trap: numbers that may not start with `0`, strings that may not hold a raw newline, `\u` escapes that come in pairs. A library people can trust gets all of them right, tells you *where* the input is broken, and survives hostile input. This is the first Hero Project: several files, a real specification, no hand-holding.

## What to build

Two header files:

- **`parser.hpp`**: `json::parse(std::string_view)` returns a `ParseResult` with the `Value`, or an `Error` with a 1-based `line` and `column` (in bytes) and a `message`.
- **`writer.hpp`**: `json::to_string(value)` for compact text, `json::to_string(value, 2)` for pretty text with 2 spaces per level.

`value.hpp` (read-only) has the `Value` type: a `std::variant` of null, bool, double, string, array and object (`std::map`, so keys come out sorted), the `Error` kinds, and `ParseResult`. Read it first. The skeleton in `parser.hpp` already handles `null`, `true`, `false` and whitespace.

## The grammar

```
value    = object | array | string | number | true | false | null
object   = '{' [ string ':' value { ',' string ':' value } ] '}'
array    = '[' [ value { ',' value } ] ']'
number   = [ '-' ] ( '0' | digit1-9 { digit } ) [ '.' digit { digit } ] [ ('e'|'E') [ '+'|'-' ] digit { digit } ]
string   = '"' { any character except '"', '\' and below U+0020 | escape } '"'
escape   = '\' ( '"' | '\' | '/' | 'b' | 'f' | 'n' | 'r' | 't' | 'u' hex hex hex hex )
white    = space | tab | newline | carriage return   (allowed between any two tokens)
```

Details people get wrong:

- `01`, `+1`, `.5`, `1.` and `-` are not numbers. In `01` the number is `0`; the `1` is trailing garbage.
- A number that is valid but too big for a double (`1e999`) is an error, `NumberOutOfRange`. One that is too small rounds to zero and is fine.
- `\u00e9` becomes the UTF-8 bytes of é. A **surrogate pair** (`\ud83d\ude00`) is *one* character (U+1F600, four UTF-8 bytes). A lone high or low surrogate is an error. `\u0000` is allowed (a string with a NUL byte).
- Bytes of 0x80 and above in a string are passed through; you do not validate UTF-8.
- A repeated key in an object: the last one wins.
- Nesting deeper than `kMaxDepth` (128) is `TooDeep`, so that `[[[[…` with a hundred thousand brackets cannot overflow the stack.

## Errors are values

Report the **first** error. The position is where the parser was looking when it gave up, with these rules (the tests check them exactly):

| Error | When | Position |
|---|---|---|
| `UnexpectedEnd` | the text ended while something was still needed (also an unclosed string, `\` at the end) | just after the last character |
| `UnexpectedCharacter` | a character that cannot be here (`[1,]`, `{a:1}`, `tru`+`x`) | that character |
| `BadNumber` | `-`, `1.`, `1e+` and so on | where a digit was needed (also at the end of the text) |
| `NumberOutOfRange` | `1e999` | the first character of the number |
| `BadString` | a raw control character in a string | that character |
| `BadEscape` | `\x`, a short or non-hex `\u`, a lone surrogate | the **backslash** of the bad escape |
| `TooDeep` | the 129th nested `[` or `{` | that bracket |
| `TrailingContent` | anything but whitespace after the document | its first character |

A `\n` starts a new line (so `\r\n` works too); the column counts bytes, starting at 1. After a failure `value` is null.

## The writer

- Whole numbers print without a fraction (`3`, not `3.0`; `-0` stays `-0`). Everything else prints with the **fewest digits that read back as exactly the same double**: `0.1` stays `0.1`, `0.1+0.2` is `0.30000000000000004`. Tip: try `%.1g`, `%.2g`, … up to `%.17g` and stop at the first one that `strtod` reads back unchanged. NaN and infinity have no JSON form: write `null`.
- Strings escape `"`, `\`, `\b`, `\f`, `\n`, `\r`, `\t`; other control characters below U+0020 become `\u00xx` (four lowercase hex digits). Everything else, including `/` and all bytes from 0x7f up, is written as is.
- Pretty printing: a newline and `indent × level` spaces before each element and before the closing bracket, `": "` after a key, and `[]` / `{}` for empty containers. `indent = 0` still breaks lines.
- The big property: for every value `v`, `parse(to_string(v))` gives `v` back, and `to_string(parse(text))` is stable.

## The tiers

- **Works**: valid documents of every kind, both directions, pretty printing and round trips.
- **Robust**: every error kind with its exact position, the depth limit, the number corner cases, 400 random round trips.
- **Fast**: values are moved, never copied (the `Value` type counts copies); the parser makes a bounded number of allocations per record; a long string is built in one buffer; the writer appends to one string; a 20 MB document finishes in time.
- **Clean**: errors are values (no `throw`/`try`/`catch`/`goto`), no raw `new`/`delete`, no `std::stod`-style throwing conversions or streams, and the parser reads a `std::string_view`.

## How to approach it

Build it in layers, running after each: numbers → strings with plain text → escapes → arrays → objects → error positions → the depth limit → the writer. A recursive-descent parser is one function per grammar rule, each returning `true` or setting the error and returning `false`.
