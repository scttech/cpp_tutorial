# Capstone: Inventory Tracker

Build a small command-line program that keeps track of stock. It reads commands from standard input, one per line,
and prints a line for each result. This brings together everything in Module 2: a `std::vector` of structs, strings
and parsing, references, and (the point of the **Clean** tier) **no raw `new` and `delete` anywhere**. The vector owns its
elements; you never manage a lifetime by hand.

## The commands

Words on a line are separated by any amount of whitespace. A blank line is ignored. A *name* is a single word.
Command words are case-sensitive.

| Command | What it does | Output on success |
|---|---|---|
| `add <name> <qty> <price>` | Add `qty` units. If the item exists its quantity goes up and **its price becomes the new price**. Otherwise it is created. | `added <qty> <name> (now <total qty>)` |
| `remove <name> <qty>` | Take `qty` units out. An item whose quantity reaches 0 is **deleted**. | `removed <qty> <name> (now <left>)` |
| `price <name> <price>` | Set the price of an existing item. | `price of <name> is now <price>` |
| `find <name>` | Show one item. | `<name>: <qty> @ <price>` |
| `report` | List every item **sorted by name** (plain `<` on strings), then the total value. | see below |
| `quit` | Stop. (The end of the input also stops the program.) | nothing |

All prices and values are printed with **exactly two decimals**.

`report` prints one line per item and then the total:

```
apple x4 @ 1.50 = 6.00
bolt x10 @ 0.25 = 2.50
total: 8.50
```

The value of an item is `qty * price`. With no items it prints `inventory is empty` instead.

## Errors

Every error is one line starting with `error: ` and the program carries on with the next line.

| Situation | Message |
|---|---|
| Wrong number of words for a command (including extra words after `report` or `quit`) | `error: usage: add <name> <qty> <price>`, `error: usage: remove <name> <qty>`, `error: usage: price <name> <price>`, `error: usage: find <name>`, `error: usage: report`, `error: usage: quit` |
| A quantity or price that is not a number (the **whole** word must be a number: `12x`, `1.5` for a quantity, `abc`, `nan`, `inf` all count as "not a number"; so does a quantity too big for an `int`) | the usage message of that command |
| Quantity is zero or negative | `error: quantity must be a positive integer` |
| Price is negative | `error: price must not be negative` |
| `add` would push a quantity above `INT_MAX` | `error: quantity too large` (nothing changes) |
| Item does not exist (`remove`, `price`, `find`) | `error: unknown item <name>` |
| `remove` more than is in stock | `error: not enough <name> (have <qty>)` (nothing changes) |
| Anything else | `error: unknown command <word>` |

**Checking order** matters when more than one thing is wrong. For `add`: the argument count, then both numbers must
parse, then the quantity is checked, then the price. For `remove`: count, parse, quantity, then the item exists, then
there is enough. For `price`: count, parse, price is not negative, then the item exists.

A price of `0` is fine, and so is `-0` (it is zero, and must be printed as `0.00`, never `-0.00`).

## Hints for the design

- Keep the data in a `std::vector<Item>`; find items with `std::find_if` (or a loop). Remember what you learned about
  invalidation: after `erase`, any pointer or iterator into the vector is stale, so read what you need first.
- `std::from_chars` is the right tool to parse a number *without* exceptions, but it stops at the first character it
  cannot use and it happily accepts `nan` and `inf` for a `double`. Check where it stopped, and check `std::isfinite`.
- Splitting a line into words is a job for `std::istringstream` and `>>`.
- `report` should sort a **copy**; the stored order does not matter to the user.
- Keep all state local to `main` and the functions it calls (no global containers): the Clean tier checks that the
  program leaves nothing allocated behind.

## Tiers

- **Works**: the commands behave as described.
- **Robust**: whitespace and line endings, every error and the order of checks, huge and tiny values, a thousand items.
- **Clean**: `main.cpp` does not use `new`, `delete`, `malloc` or `free` (the test scans your source), and a session
  leaves no memory behind.
