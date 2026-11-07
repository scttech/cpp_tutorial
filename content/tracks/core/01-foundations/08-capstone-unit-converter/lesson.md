# Capstone: Unit Converter CLI

You will build a small command-line program that reads commands from standard input, converts between units, and
reports problems gracefully. It uses everything from Module 1: types, operators, control flow, functions, a
lookup table, and robust I/O.

## The program

Read **lines** from stdin until the input ends or a `quit` command arrives. Blank lines are ignored. Each other line is a
command made of whitespace-separated words. (All output, including errors, goes to **stdout**.)

### `convert <value> <from> <to>`

Convert `<value>` from one unit to another and print:

```
<value as typed> <from> = <result with exactly 4 decimals> <to>
```

Examples:

```
convert 1 km m           ->   1 km = 1000.0000 m
convert 100 C F          ->   100 C = 212.0000 F
convert 10 lb kg         ->   10 lb = 4.5359 kg
convert 1.50 km m        ->   1.50 km = 1500.0000 m
```

### `quit`

Stop immediately. Nothing after it is processed.

### Units (case-sensitive)

| Dimension | Units | Relation |
|---|---|---|
| Length | `m`, `km`, `mi`, `ft` | `1 km = 1000 m`, `1 mi = 1609.344 m`, `1 ft = 0.3048 m` |
| Mass | `kg`, `g`, `lb` | `1 g = 0.001 kg`, `1 lb = 0.45359237 kg` |
| Temperature | `C`, `F`, `K` | `K = C + 273.15`, `F = C * 9/5 + 32` |

Temperatures are not simple multiplications: they have offsets. Think about converting through a common base
unit (meters, kilograms, kelvin) so you do not need a function for every pair of units.

### Errors

Print one line, then carry on with the next command. Check in **this order**:

| Problem | Output |
|---|---|
| Unknown command word | `error: unknown command '<word>'` |
| `convert` without exactly 3 arguments | `error: usage: convert <value> <from> <to>` |
| `<value>` is not a finite number (`abc`, `5x`, `nan`, `inf`) | `error: invalid number '<value>'` |
| `<from>` is not a known unit | `error: unknown unit '<from>'` |
| `<to>` is not a known unit | `error: unknown unit '<to>'` |
| The units measure different things (`km` to `kg`) | `error: cannot convert <from> to <to>` |
| A temperature below absolute zero (less than 0 K, allowing 1e-9 of rounding slack) | `error: below absolute zero` |

Two output details that matter:

- If the result rounds to zero, print `0.0000`, never `-0.0000`.
- Print the value exactly as the user typed it (`1.50` stays `1.50`).

## Hints on design (not on the answer)

- Model the data, not the pairs: a table from unit name to (dimension, factor).
- Separate **parsing a line**, **converting a number**, and **printing** into functions. Each is easy to test and reason about.
- `std::from_chars` for `double` accepts `nan` and `inf`. You decide what to do about that: see the error table.
- Reading a number from text and checking it consumed the whole token was the key trick in lesson 7.

The **Works** tests check the happy path. **Robust** checks every error row above and a number of awkward inputs.
