# Fast path: what's different in C++

- **`std::map` is a sorted tree; `std::unordered_map` is a hash table.** Use the first when order or ranges matter (`lower_bound` finds the start of a `RANGE` in O(log n)), the second for exact lookups (the COUNT index). Python's `dict` is hash-only; Java's `TreeMap` and `HashMap` are the same two structures.
- **Heterogeneous `std::variant` instead of an inheritance tree.** The set of commands is closed, so `std::visit` with one overload per command is checked by the compiler: add a command and every `visit` that forgets it stops compiling.
- **Move, don't copy.** `set(Text key, Text value)` takes its arguments *by value* so a caller with a temporary pays nothing and a caller with a variable pays one copy, explicitly. The container then `std::move`s them in. `Text` counts copies for exactly this reason.
- **Undo log vs snapshot.** A snapshot (copy on BEGIN) is O(size of data) per transaction; an undo log is O(size of the changes). Databases use logs (write-ahead logs, rollback journals, MVCC versions) for the same reason.
- **`std::from_chars`** parses numbers without exceptions, locale or allocation, and tells you where it stopped; `stoll` throws on bad input and accepts trailing garbage and a leading `+`.
- **`std::optional<Text>`** in the undo entry says "the key did not exist" without a magic value.
