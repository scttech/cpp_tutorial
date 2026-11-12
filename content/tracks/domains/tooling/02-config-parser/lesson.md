# A Config File Parser with Line and Column Errors

> **You're on the developer-tools team.** Every product has a config file, and the first thing a user does with it is break it: a missing quote, a path with backslashes, `port = 80x`. A parser that answers "invalid config" has failed; one that answers `app.conf:12:8: invalid number`, shows the line and puts a caret under the `x`, has saved a support ticket. You will write a TOML-like parser, small enough to read in one sitting, whose errors are as good as a compiler's.

## The shape of a hand-written parser

No regex, no parser generator: a **cursor** over the text (`index`, `line`, `start of this line`) and one function per grammar rule, each one consuming input and returning success or failing with an error. This is *recursive descent*, and it is how real compilers and JSON/TOML libraries are written. The rules:

```
document  = { blank | comment | section | setting }
section   = "[" keypath "]"  end-of-line
setting   = keypath "=" value  end-of-line
value     = string | bool | number | array
array     = "[" [ value { "," value } [","] ] "]"        (newlines and comments allowed between elements)
```

Errors are returned, not thrown. The first error ends the parse and carries a **kind**, a **line**, a **column** and a message. A parse function that fails stores the error and returns `false`; the callers just propagate.

## Errors that point at the right place

The column is the whole point, and it is easy to be off by one or point at the wrong token. The rule used here is "the character a human would underline": the opening quote of an unterminated string (not the end of the line where you noticed); the backslash of a bad escape (not the letter after it); the first digit of a malformed number; the second `.` of `a..b`; the *end of the line* for a missing `=` or value; the opening `[` of an array that never closes. Keep a `Pos` captured at the start of each construct, because by the time you detect the problem the cursor has moved.

## Numbers deserve a grammar

`std::stod("1.5abc")` returns 1.5 and `std::stoi("08")` is 8 or 0 depending on the base. A config file wants a strict grammar, so check it by hand and use `std::from_chars` only for the final conversion:

- integers: optional sign, digits with `_` **between** digits (`1_000`), no leading zeros (`007` is an error: in some languages that means octal!), range-checked into `long long`;
- hex: `0x` + digits, no sign;
- floats: digits **on both sides** of the `.` (`1.` and `.5` are errors), optional exponent, rejected if the result overflows to infinity.

## Strings

Basic strings have escapes (`\n`, `\"`, `\uXXXX`); literal strings have none (ideal for Windows paths). Both end on their own line, and a raw control character inside is an error: otherwise a stray byte becomes part of a value, invisibly. `\uXXXX` becomes UTF-8 bytes; surrogates (`D800`-`DFFF`) are not scalar values and are refused. Bytes of 0x80 and up pass through unchanged, so UTF-8 text works without the parser knowing about it.

## Duplicates and arrays

`Config::add` refuses a path that exists, so `a = 1` twice is `DuplicateKey` at the second key. Sections are checked separately. Arrays must be homogeneous (a typed config is easier to consume) and may span lines, which is where line counting goes wrong: count a newline wherever you consume one, including inside arrays and `\r\n`.

## Showing the user

`render` prints `file:line:column: error: message`, the offending line and a caret. Keep tabs as tabs in the caret line so it lines up under any tab width.

## The tiers

- **Works**: every value type and escape, keys and sections, comments, CRLF and BOM, arrays, every error kind with its exact position, duplicates, `render`.
- **Robust**: 2000 generated documents round-trip; every one-character mutation of a valid document is handled; random garbage; number round trips and `long long` limits; deep, wide and huge inputs.
- **Fast**: a 100000-setting document in linear time; a bounded number of allocations per line; late errors found quickly.
- **Clean**: no raw memory or exceptions; no regex, streams, `stoi`/`stod`/`sscanf`.
