# Fast path: what's different in C++

- Streams: `std::cout` / `std::cerr` out, `std::cin` in. `>>` extracts whitespace-delimited tokens; `std::getline(std::cin, s)` reads a whole line.
- A failed `>>` sets the stream's **fail state** and *all later reads silently do nothing* until `clear()` (and you must also discard the bad input).
- Mixing `cin >> x` then `getline` leaves the newline behind, so the `getline` returns an empty string. Prefer reading lines and parsing them.
- Loop on the read itself: `while (std::getline(std::cin, line))`. Never `while (!cin.eof())`.
- `std::stoi` throws; `std::from_chars` (from `<charconv>`) doesn't. Check the error code **and** that the whole token was consumed.
- `std::string` stores **bytes**, not characters: `.size()` of a UTF-8 string counts bytes.
