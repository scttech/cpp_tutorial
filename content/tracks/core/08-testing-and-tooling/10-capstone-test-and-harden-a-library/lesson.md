# Capstone: Test and Harden a Library

Every tool of this module in one job. You inherit a small **CSV library** (`csv.hpp`): code that reads and writes comma-separated values, with quoting rules, Windows line endings and multi-line fields. Real CSV code is a famous source of bugs, and this copy has its share. Your job is to make it trustworthy, and to prove it.

## What you do

1. **Read the contract.** The comment at the top of `csv.hpp` is the specification: what `parse_line`, `format_line` and `parse_document` must do, down to the edge cases (an empty line, a trailing delimiter, a quote inside a quoted field, `\r\n`).
2. **Write the tests** in `csv_test.cpp` with GoogleTest. Run Tests runs them against your `csv.hpp`, and against **eight broken copies of a correct library**, each with one realistic bug. A copy that your tests miss is listed by name with a hint. (This is the mutation testing from the first lesson of the module.)
3. **Fix the library.** Your tests will fail on the buggy `csv.hpp` you were given: that is the point. Read the failures (the debug challenge taught you how), fix the code, and watch them go green. Hidden checks also look at the behavior of *your* `csv.hpp` directly, so a library that is still wrong cannot hide behind weak tests, and weak tests cannot hide behind a library that happens to work.
4. **Harden it.** The build uses `-Wall -Wextra -Wpedantic -Wconversion -Wshadow` and the AddressSanitizer and UBSan. After the build, **clang-tidy** (configured by the read-only `tidy.yaml`) and **cppcheck** look at `csv.hpp`. The library also has a performance problem that the tests of the **Fast** tier measure by counting heap allocations.

## The rules of the game

- Fix the **code**; do not hide a problem. A `NOLINT`, a `cppcheck-suppress` or a `#pragma` that silences a diagnostic is rejected by the Clean tier.
- Do not change `lint_unit.cpp` or `tidy.yaml`.
- Tests are for the **behavior in the comment**, through the public functions: do not test private details (the `detail` namespace).
- One behavior per test, a name that says what the code promises.

## The tiers

- **Works**: your tests pass on your library and catch the everyday bugs (trailing delimiter, doubled quotes, quoting when formatting); your library does the everyday things right.
- **Robust**: the edges: unclosed quote, text after a closing quote, Windows line endings, line breaks inside quotes, blank lines; your library is right on all of them.
- **Fast**: building a long line must not allocate on every field (the library is graded by an allocation budget).
- **Clean**: no compiler warning, no clang-tidy finding, no cppcheck finding, no suppression comments.

## A suggested order

1. Read the contract and write a test for the first rule. Run it. Is the library right?
2. For each rule, write the test **first**, see it fail on the broken library (if it does), then fix the library until it passes. This is the rhythm of test-driven development: red, green, then tidy up.
3. Run Tests to find out which broken copy your tests missed; each name is a rule you did not test.
4. Last, the warnings and the analyzers: their findings are listed in the Problems tab. Each one tells you what to change.

When all four tiers are green, you have done what professional C++ teams do on every change: tests that can fail, a fixed library, warnings at zero, static analysis at zero, sanitizers clean.
