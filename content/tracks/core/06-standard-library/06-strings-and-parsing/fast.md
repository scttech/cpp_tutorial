# Fast path: what's different in C++

- **`std::from_chars(first, last, value)`** (`<charconv>`): returns `{ptr, ec}`. No exceptions, no allocation, no locale, no whitespace skipping, no `+` sign. It parses the longest valid prefix: **check `ec == std::errc{}` and `ptr == last`** for a whole-string parse. Out of range is `std::errc::result_out_of_range`; no digits at all is `std::errc::invalid_argument`. Floating-point `from_chars` also accepts `inf`/`nan`.
- **`std::to_chars(first, last, value[, base or format])`** writes into your buffer and returns `{ptr, ec}` (`ec` is `value_too_large` if the buffer is too small). Shortest round-trip form for floating-point.
- **`std::format("{:<10}|{:>5}|{:8.2f}", s, n, x)`** (C++20, `<format>`): `{}` placeholders, spec after `:` = `[[fill]align][sign][#][0][width][.precision][type]`; `<` `>` `^` align; types `d x X b o f e g s`. Checked at compile time with a literal format string. `std::print` (C++23) writes it directly.
- **`std::string_view`**: non-owning (pointer, length), constant-time `substr`/`remove_prefix`, `find`, `starts_with`/`ends_with` (C++20). Does not guarantee a terminating `\0`. Do not return one that views a local string or a temporary.
- **Avoid `std::regex`** for hot or simple parsing: slow to construct and to match, large code, can overflow the stack on long input. A few `find`/`from_chars` calls are faster and clearer.
- **`atoi` returns 0 on failure** (indistinguishable from "0"), `stoi`/`stod` throw and ignore trailing junk. Prefer `from_chars` plus an explicit end check.
- **Splitting**: `find(sep, start)` in a loop gives views; keep empty fields unless you mean to drop them (C++20 `std::views::split` does the same, but gives subranges).
