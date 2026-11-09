# Static analysis: clang-tidy and cppcheck

The compiler tells you about code that is *suspicious*. A **static analyzer** goes further: it reads the program without running it and looks for whole categories of *bugs*, *performance problems* and *bad habits* that compile without a single warning. Two free, open-source tools are the standard answer in C++:

- **clang-tidy** (part of LLVM) uses the real Clang parser, so it understands your code exactly like the compiler does. It has several hundred **checks**, in families: `bugprone-*` (likely bugs), `performance-*`, `modernize-*` (use newer language features), `readability-*`, `cppcoreguidelines-*` (the C++ Core Guidelines), `clang-analyzer-*` (a path-sensitive analysis of memory and null pointers), `misc-*`, and more. Many checks can also **fix** the code automatically (`--fix`).
- **cppcheck** has its own parser and its own checks. It is fast, has few false positives, and finds some things clang-tidy does not (array bounds, uninitialized members, resource leaks, some UB). It also works on code that does not compile.

They overlap and complement each other. Teams often run both in CI, together with the compiler's warnings and the sanitizers (the next lesson).

## How they are used

```
clang-tidy -p build/ src/inventory.cpp          # -p: the folder with compile_commands.json, so that tidy compiles the file like the build does
clang-tidy --checks='-*,bugprone-*' file.cpp -- -std=c++20 -Iinclude
cppcheck --enable=warning,performance,portability --inline-suppr src/
```

- **`compile_commands.json`** is a list of the exact compiler command of each file; CMake writes it with `-DCMAKE_EXPORT_COMPILE_COMMANDS=ON`. Without it, tidy does not know your include paths and flags.
- **The configuration** goes in a file called **`.clang-tidy`** at the root of the project (YAML: a `Checks:` list where a leading `-` removes checks and `*` is a wildcard, and `WarningsAsErrors:`). In this lesson it is called `tidy.yaml` and is read-only. A good start: `bugprone-*`, `performance-*`, `modernize-*`, `readability-*`, minus the checks that your team finds noisy.
- **Findings can be wrong** (a *false positive*), or right but intended. Silence one narrowly, on its own line, with the check's name, and say why: `// NOLINT(bugprone-use-after-move) the string is reassigned below`, or `// cppcheck-suppress accessMoved`. Never silence a check to get past a real bug, which is why this lesson's Clean tier rejects both comments.

Static analysis finds bugs **without** a test that triggers them, but it can also be wrong, and it cannot know what your function is *supposed* to do. The compiler warnings, the analyzers, the tests and the sanitizers each catch things that the others miss.

## Your task: eight findings, eight bugs

`inventory.hpp` holds eight small pieces of code. Each **compiles without a warning** and each is broken. The comment above each says what it should do. Press **Run Tests**: after the build, clang-tidy (with the read-only `tidy.yaml`) and cppcheck run over your files, and every finding appears in the **Problems** tab, with the file, the line and the check's name, for example `[bugprone-use-after-move]`. Look the name up (clang.llvm.org/extra/clang-tidy/checks): each check has a page with an example and the fix.

| piece | what the tools say | what is really wrong |
|---|---|---|
| `announce` | `bugprone-use-after-move`, cppcheck `accessMoved` | the string was moved into the log, then read: it is empty |
| `total_length` | `performance-unnecessary-value-param`, `readability-container-size-empty` | the whole vector is copied on every call |
| `longest` | `performance-for-range-copy` | every string is copied while looking |
| `average` | `bugprone-integer-division` | `7 / 2` is `3`, even though the function returns a `double` |
| `unit_price` | `bugprone-branch-clone` | both branches do the same: the discount applies to every order |
| `bucket_counts` | cppcheck `arrayIndexOutOfBounds` | the loop writes one past the end; the sanitizer in the build catches it at run time too |
| `Inventory::find` | `modernize-use-nullptr` | `NULL` is a macro for `0`; `nullptr` has its own type and cannot be mistaken for an integer |
| `Stats` | `cppcoreguidelines-pro-type-member-init`, cppcheck `uninitMemberVar` | `Stats()` leaves `count` and `sum` uninitialized |

(Do not read this table before you try: the point is to learn to read the tools' output.)

- **Works** and **Robust**: the functions do what their comments say, also at the edges.
- **Fast**: `total_length` and `longest` do not copy their arguments (the tests count heap allocations).
- **Clean**: clang-tidy reports nothing, cppcheck reports nothing, and the file has no `NOLINT` or `cppcheck-suppress` comment.
