# Fast path: what's different in C++

- **`std::string_view` is a pointer and a length.** Copying it is two words; making a substring (`substr`) is O(1) and allocates nothing. It does **not own** the characters: that is the whole bargain. Rust enforces the lifetime with the borrow checker; C++ leaves it to you, and AddressSanitizer (Module 8) is how you find the mistakes.
- **Never construct a `string_view` from a temporary `std::string`**: `std::string_view v = make_string();` dangles at the semicolon. Likewise returning a view of a local string.
- **`std::from_chars`** (C++17) is the fast, locale-independent, non-throwing, non-allocating integer parser: it returns a struct with the end pointer and an error code. It is what `std::stoi` should have been. Always check that `ptr == end` or you accept `"5x"`.
- **Strict parsers for the network.** Protocol bugs are security bugs: a parser that is lenient about `+5`, ` 5` or `5x` disagrees with the one on the other side, and attackers live in that gap (*parser differentials*). Define the grammar and reject everything else.
- **Checksums** here are 8-bit sums (FIX); real protocols use CRC-32/CRC-32C (Ethernet, iSCSI) or cryptographic MACs. The principle is the same: detect corruption by recomputing over the received bytes, with *unsigned* arithmetic (`static_cast<unsigned char>`), since `char` may be signed.
- **Latency-critical parsers** (the ones in exchanges) avoid even the vector of fields: they parse into a fixed array or a callback-per-field "SAX" style.
