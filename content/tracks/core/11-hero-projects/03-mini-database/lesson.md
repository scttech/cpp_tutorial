# Hero Project: Mini Database

> **Four hundred lines that behave like a real database.** A key-value store with a query language, an index, an ordered range scan and transactions that nest. Redis, SQLite and every other store you have used are built from these parts: a *parser* for the command line, a *data structure* for the data, an *index* for the questions you ask often, and an *undo log* so that a failed operation can be taken back.

## The commands

```
SET key value        -> OK                     DEL key     -> (integer) 1 if it existed, else 0
GET key              -> "value" or (nil)       COUNT value -> (integer) how many keys hold exactly this value
RANGE lo hi          -> the keys lo <= key <= hi, ascending, as a list
INCR key             -> (integer) the new value; a missing key counts as 0
BEGIN                -> OK       open a transaction (they nest)
ROLLBACK             -> OK       undo the changes since the matching BEGIN   | ERR NO TRANSACTION
COMMIT               -> OK       make ALL open transactions permanent        | ERR NO TRANSACTION
```

Command names are case-insensitive; keys and values are not. Arguments are separated by spaces or tabs. An argument is a **bare word** (no quote inside it) or a **"quoted string"** that may contain spaces and the escapes `\"`, `\\`, `\n`, `\t` (`SET "first name" "Ada Lovelace"`). Keys sort **bytewise**: `"B" < "a"`, and `"a" < "ab"`.

`INCR` reads the value as a whole number (an optional `-`, then digits: no `+`, spaces or decimals). Anything else, or a number that does not fit a `long long`, is the error `value is not an integer`; incrementing the largest `long long` is `increment would overflow`; in both cases nothing changes.

## Three files

| File | You write | Notes |
|---|---|---|
| `query.hpp` | `parse_command(line)` | line → `Command` (a `std::variant`), or a `ParseError` with a byte offset |
| `store.hpp` | `class Store` | the data, the COUNT index, the transactions |
| `database.hpp` | `class Database` | parse → run → `Reply` |

`types.hpp` (read-only) defines the commands, the errors, `Reply`, and `Text`: a `std::string` that **counts how often it is compared and copied**, so the tests can measure your design rather than guess at it.

### Parse errors (the tests check kind and offset)

| Error | When | Position |
|---|---|---|
| `Empty` | nothing but whitespace | the end of the line |
| `UnknownCommand` | first token is no command (a *quoted* name is not a command) | the first token |
| `WrongArity` | wrong number of arguments | the first token |
| `UnterminatedString` | a quote is never closed (also a backslash as the last character) | the opening quote |
| `BadEscape` | a backslash followed by anything but `" \ n t` | the backslash |
| `StrayQuote` | a quote inside a bare word, or anything glued to a closing quote (`"a"b`) | that character |

Order: **quote problems anywhere in the line come first** (scan the whole line, left to right), then `Empty`, `UnknownCommand`, `WrongArity`. A line that does not parse gives an error reply and changes nothing.

### Transactions: the undo log

The simple design copies the whole map on `BEGIN`. It works, and it is exactly what the Fast tier rejects: `BEGIN` on a million keys must not touch a million keys. The real design: **BEGIN is free; every change made inside a transaction records how to undo itself** (the key and what it held before, or "it did not exist"). `ROLLBACK` replays its transaction's log *backwards*. `COMMIT` simply forgets the logs. Think about what must be logged for `SET` on an existing key, `SET` on a new key, and `DEL`, and about what `COUNT` must report after each.

## The tiers

- **Works**: every command, quoting, ordering, `INCR`, nested transactions.
- **Robust**: all parse errors with offsets, the error order, every `INCR` edge, rollback corner cases (a key changed many times, deleted then re-created, created then deleted), three nesting levels, and 3000 random commands compared with a snapshot-copying model.
- **Fast**: moving instead of copying, ordered-tree lookups, an index for `COUNT`, `RANGE` from `lower_bound`, free `BEGIN`, a rollback that costs what the transaction changed, a cheap `COMMIT`, few allocations when parsing.
- **Clean**: no raw `new`/`delete`, no exceptions, no `stoi`-style conversions or streams, commands dispatched through the variant.

## How to approach it

Start with the parser and `SET`/`GET`, then `DEL`/`COUNT`/`RANGE`, then the transaction commands, `INCR` last. In `Store`, write the *unlogged* operations first (`put`, `drop`) that keep the data and the COUNT index consistent; the logged `set`/`del` and `rollback` are then small.
