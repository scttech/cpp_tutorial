# Typed Register Fields

> **You're a firmware engineer** bringing up a new UART. The datasheet describes a control register as fields: `ENABLE` in bit 0, `PARITY` in bits 2:1, `STOP` in bit 3, `BAUD` in bits 15:8. The classic C code is `CTRL = (CTRL & ~0x6) | (parity << 1);` repeated everywhere with hand-typed constants, and the classic bugs are a mask one bit too narrow, a value that spills into the neighbouring field, and a *read-modify-write on a register where a read has side effects*. You will build the small layer that makes these mistakes impossible to write.

## Fields: masks and shifts, once

`Field<T, Offset, Width>` is a type. Its constants come from the template arguments at compile time:

- `max_value = 2^Width - 1`, `mask = max_value << Offset`
- `get(reg) = (reg & mask) >> Offset`
- `insert(reg, v) = (reg & ~mask) | ((v << Offset) & mask)`: clear the field's old bits **and** truncate `v`, or a too-wide value corrupts the field next door.
- **A field as wide as the register** (`Width == 32`) must not compute `1 << 32`, which is undefined. Build the mask from an all-ones value shifted right instead: `~T{0} >> (digits - Width)`.
- On 8- and 16-bit types, `~mask` is promoted to `int`; cast back to `T`.

`static_assert` in the class rejects fields that do not fit in the register at *compile time*: a wrong datasheet transcription is a build error, not a mystery on the bench.

## Registers: hardware accesses are the cost

On real hardware each read and write of a register is a bus transaction, and some registers have **side effects**: reading a status register may clear it, writing a FIFO register pushes a byte. So the layer counts accesses and does each operation with the fewest possible:

| Operation | Reads | Writes |
|---|---|---|
| `read_field<F>()` | 1 | 0 |
| `write_field<F>(v)`, `set_bits`, `clear_bits`, `toggle_bits` | 1 | 1 |
| `modify(set<F1>(a), set<F2>(b), ...)` | **1** | **1** |
| `write_one_to_clear(mask)` | **0** | 1 |
| `wait_for<F>(expected, n)` | up to n | 0 |

`modify` takes any number of assignments (a variadic template) and applies them all to **one** local copy with a fold expression: `((v = Assigns::field::insert(v, assigns.value)), ...);`.

**Write-1-to-clear** flags (interrupt status registers) are the trap: writing a 1 clears the flag, writing a 0 does nothing. If you "clear flag 1" with a read-modify-write you read `0b1011`, set... and write back all the *other pending flags as 1s*, clearing interrupts you never handled. The correct operation writes **only the mask**.

`wait_for` polls at most `max_polls` times, stops reading the moment the field matches, and does not touch the hardware at all for `max_polls == 0` (a timeout of zero must not cost a bus cycle).

## The tiers

- **Works**: masks at the edges (bit 0, the top bits, full width, 8/16-bit types), `get`/`insert` with truncation, constant-expression use, exact read/write counts for each operation.
- **Robust**: a table of offset/width combinations against an independent computation, narrow registers, W1C, composed use.
- **Fast**: hardware-access counts over 1000 operations.
- **Clean**: no raw memory, exceptions, macros, standard containers or `iostream` (this is firmware), and `constexpr` throughout.
