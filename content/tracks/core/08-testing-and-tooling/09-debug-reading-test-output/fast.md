# Fast path: what's different in C++

- **Read GoogleTest output from the top of each failure**: `file:line: Failure`, then the compared expressions with `Which is:` (the actual values). `EXPECT_THROW` failures name the expected type and say "throws a different exception type" or "throws nothing". Use `--gtest_filter=Suite.Name` to run one test, `--gtest_break_on_failure` to stop in a debugger.
- **Sanitizer reports end the program** (`halt_on_error`/`-fno-sanitize-recover`): fix the first one, re-run. UBSan: `file:line:col: runtime error: <rule broken>`. ASan: the error kind (`heap-use-after-free`, `heap-buffer-overflow`, `stack-use-after-scope`, `attempting double-free`), the access, and three stacks (use / free / allocation). Read the first frame in your own code.
- **A reference or iterator into a `std::vector` is invalidated by anything that may reallocate** (`push_back`, `insert`, `resize`, `reserve`). Return by value, return an index, or keep the data in a container whose elements do not move (`std::deque`, `std::list`, `std::map`).
- **Signed integer overflow is undefined behavior**, not wrap-around: the check must come *before* the operation (`if (a > INT_MAX - b)`), never after it. Unsigned arithmetic wraps by definition.
- **Exception types matter**: `catch`/`EXPECT_THROW` match by type and base classes (`std::invalid_argument` derives from `std::logic_error`, not the other way round). Choose the most specific standard type that fits.
- **All-or-nothing (the strong exception guarantee)**: do the steps that may throw first, then commit; or undo what was done in a `catch (...) { rollback; throw; }`. Do not forget the side effects other than the balance (history, counters, caches).
- **Static counters for ids** (`static inline int next_id_`) are shared by all instances and are not thread-safe (use `std::atomic<int>` for concurrent code).
- **When a test fails, check the specification first, then the code, last of all the test.**
