# CRC with a Compile-Time Table

> **You're a firmware engineer.** Every frame on the serial link ends with a checksum, the flash image carries one, and the bootloader refuses to run code whose CRC does not match. The datasheet says "CRC-16/MODBUS", the peer implements "CRC-16/CCITT" and the first week of integration goes to finding out why 1 byte in 65536 is wrongly rejected. You will write one engine that does **any** 8-, 16- or 32-bit CRC from its published parameters, with a 256-entry lookup table that **the compiler computes**, so there is no start-up code, no RAM for the table (it lives in flash), and the whole checksum of a constant string can be evaluated at build time.

## The model: five parameters

The "Rocksoft model" (from a 1993 paper by Ross Williams) describes every CRC with:

| Parameter | Meaning |
|---|---|
| `poly` | the generator polynomial, without its top bit |
| `init` | the starting register |
| `reflect_in` | process each input byte least-significant-bit first |
| `reflect_out` | bit-reverse the register at the end |
| `xor_out` | XOR the result with this |

`types.hpp` gives the catalogue entries and the **check value** of each: the CRC of the nine ASCII characters `123456789` (CRC-32: `0xCBF43926`). Every CRC library in the world is tested against these.

## Bit by bit, then by table

The bitwise algorithm handles one bit at a time (8 shifts per byte). The table-driven version precomputes what 8 shifts do to each of the 256 possible byte values, so each input byte costs one lookup, one XOR and one shift:

- **Direct (not reflected):** `r = table[((r >> (bits-8)) ^ b) & 0xFF] ^ (r << 8)`, table built from `i << (bits-8)` with shifts left.
- **Reflected:** keep the register *bit-reversed* so that LSB-first input needs no per-byte reversal: `r = table[(r ^ b) & 0xFF] ^ (r >> 8)`, table built with shifts right using `reflect(poly)`.

The traps, every one of which has shipped in real firmware:

- **The reflected register starts from `reflect(init)`**, not `init`. It makes no difference when `init` is `0` or all-ones, which is why many implementations get away with the bug until a protocol uses `0x1D0F`.
- **`reflect_in != reflect_out`:** the register is in the "input" direction, so it must be bit-reversed at the end *only if the two flags differ*.
- **8-bit CRCs:** `r >> 8` and `r << 8` shift everything out; with `uint8_t` promoted to `int` you must cast back.
- **`value()` must not change the state**, so you can read a running CRC and keep feeding bytes.

## The compile-time table

```cpp
static constexpr std::array<T, 256> table = make_table();
```

`make_table()` is a `constexpr` function (loops are allowed in `constexpr` since C++14); the compiler runs it during the build and stores the result in read-only memory. The parameters travel as a **non-type template argument** of class type (C++20): `Crc<std::uint32_t, kCrc32>`, so each algorithm gets its own table and its own code, with `if constexpr (P.reflect_in)` choosing the loop shape at compile time at zero run-time cost.

## The tiers

- **Works**: `reflect`, entries of famous tables, **13 catalogue check values** across all three widths and both directions, the CRC of nothing, `span` and `string_view`, incremental use, `reset`, evaluation at compile time.
- **Robust**: seven unusual parameter sets (mixed reflection, asymmetric `init`) against an independent bit-by-bit model, all standard algorithms on random data, random chunking of messages (with `value()` peeks in between), single-bit-error detection, the table entry by entry.
- **Fast**: no allocation, `sizeof(Crc32) == 4` (the table is shared), 8 MB at once and in 4 KB pieces.
- **Clean**: no raw memory or exceptions; no containers except `std::array`/`std::span`; `make_table` is `constexpr`.
