# Fast path: what's different in C++

- `std::string` owns a resizable byte buffer (short strings live inside the object; long ones on the heap). **`size()` is bytes, not characters**: UTF-8 characters take 1-4 bytes, and every byte of a multi-byte character is a *negative* `char`.
- `<cctype>` functions (`isspace`, `isalpha`, `toupper`...) need an `unsigned char` value: `std::isspace(static_cast<unsigned char>(c))`.
- `std::string_view` = pointer + length, **non-owning**. Slicing and passing views never copies. Use it for read-only string parameters (it accepts literals and strings alike).
- **Dangling:** a view must not outlive the string it points into. Never return a `string_view` of a local `std::string`; return a `std::string` instead.
- Build a result of known size with `reserve` and `+=`: one allocation instead of repeated growth. Never `erase(0, n)` from the front of a big string in a loop (quadratic): walk an index forward instead.
- UTF-8 continuation bytes are `10xxxxxx`; count the bytes that are not.
