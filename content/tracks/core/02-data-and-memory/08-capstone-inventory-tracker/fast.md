# Fast path

A stdin-driven inventory: `add <name> <qty> <price>`, `remove <name> <qty>`, `price <name> <price>`, `find <name>`, `report`, `quit`. One output line per command, errors as `error: ...` (see the full tables in the Lesson tab: usage errors, `quantity must be a positive integer`, `price must not be negative`, `quantity too large`, `unknown item`, `not enough <name> (have N)`, `unknown command`).

Key points: the **whole** word must parse (`std::from_chars` + check `ptr == end`, and `std::isfinite` because it accepts `nan`/`inf`); validate in order (count, parse, quantity, price); reject `INT_MAX` overflow before adding; `-0` prints as `0.00`; `report` sorts a copy by name and prints `total: ...`; an item that reaches 0 is erased (read what you need before erasing). Two decimals everywhere.

**Clean tier:** no `new`/`delete`/`malloc`/`free` in `main.cpp` and no leaked memory: let `std::vector` and `std::string` own everything, and keep state local.
