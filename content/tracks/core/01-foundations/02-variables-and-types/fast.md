# Fast path: what's different in C++

- Static typing; `auto` deduces the type, `const` / `constexpr` for immutability and compile-time values.
- Prefer brace initialization `int x{5};`; it rejects narrowing (`int x{3.9};` is an error).
- **Uninitialized locals are undefined behavior**, not zero or null as in most languages. Always initialize.
- `int / int` is integer division (truncates toward zero). `5 / 9` is `0`. Use a `double` operand.
- Type sizes are implementation-defined (`int` is usually 32 bits, `long` varies by platform). Use `<cstdint>` for exact widths.
- `char` is a one-byte integer type; whether plain `char` is signed is implementation-defined.
