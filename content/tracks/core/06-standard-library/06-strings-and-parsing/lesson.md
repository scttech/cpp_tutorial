# Strings, formatting and parsing

Reading numbers from text and writing text from numbers is where many programs are slow, wrong, or both. C++ has several ways; this lesson is about the modern ones, and why.

| Job | Old way | Problem | Modern way |
|---|---|---|---|
| text to int | `atoi`, `stoi`, `istringstream` | `atoi` returns 0 on error (indistinguishable from "0"); `stoi` throws and accepts trailing junk and leading spaces; streams are slow, allocate, and depend on the locale | **`std::from_chars`**: no exceptions, no allocation, no locale, reports where it stopped |
| number to text | `sprintf`, `to_string`, `ostringstream` | `sprintf` is not type safe; the rest allocate | **`std::to_chars`** into a buffer; **`std::format`** for anything fancier |
| pieces of a string | `substr` (copies) | an allocation per piece | **`std::string_view`**: a pointer and a length |
| pattern matching | `<regex>` | very slow to compile and run, huge code size, stack overflows on long inputs | hand-written parsing with `string_view` and `from_chars`, or a dedicated library |

## `std::from_chars`

```cpp
int value = 0;
const char* end = text.data() + text.size();
auto [stop, ec] = std::from_chars(text.data(), end, value);     // structured bindings of a from_chars_result
if (ec != std::errc{} || stop != end) return std::nullopt;       // check the error code AND that it consumed everything
```

- It stops at the first character that cannot be part of the number and tells you where (`stop`). **"12abc" parses as 12 with `stop` pointing at `a`:** it is not an error unless you check `stop == end`.
- It accepts a leading `-` but **not** `+`, and **no leading whitespace**.
- An out-of-range value gives `ec == std::errc::result_out_of_range` (no exception, no wrap-around).
- For floating-point it also accepts **`inf`, `infinity` and `nan`**: if those are not numbers for you, check `std::isfinite`.

## `std::to_chars` and `std::format`

`std::to_chars(first, last, value, base)` writes into a buffer you provide and returns the end: `std::string(buffer, result.ptr)`.

`std::format` (C++20) uses `{}` placeholders with a type-safe format spec after a colon: `{:<10}` left-align in 10 columns, `{:>5}` right-align in 5, `{:8.2f}` fixed with 2 decimals in 8 columns, `{:#x}` hexadecimal with a `0x`, `{:08.3f}` zero-padded. A wrong spec for the type is a **compile** error, not garbage output as with `printf`.

## `std::string_view`

A read-only view of characters you do not own: `substr`, `find`, `find_first_not_of`, `remove_prefix` all work on views without copying. The price is the lifetime rule: **a view must not outlive the string it views** (never return a `string_view` of a local `std::string`, never keep one of a temporary). That is also why `split_fields` is documented "valid while `line` is".

## Your task

Fill in `text_tools.hpp`: `parse_int`, `parse_double`, `to_hex`, `format_row`, `trim`, `split_fields`, and `parse_log_line`, which uses the others. The tiers: **Works**, **Robust** (overflow, `inf`/`nan`, views into longer strings, empty fields, malformed log lines, a Windows line ending), **Fast** (the number parsers and `trim` must not allocate; `parse_log_line` may allocate only the two strings of its result).
