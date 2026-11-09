# Formatting, CMake and ctest

Three more tools complete the picture. They are about *how a project holds together* as much as about C++: a formatter ends arguments about layout, a build system makes the project reproducible, and `ctest` makes "run all the tests" one command that a machine can run.

## clang-format: no more arguments about style

**clang-format** (from LLVM) rewrites C++ code into a consistent layout, from a configuration file. Indentation, where braces go, line length, spaces around operators, how `#include`s are ordered: the tool decides, always the same way, and nobody reviews layout again.

```
clang-format -i src/*.cpp include/*.hpp          # rewrite the files in place
clang-format --dry-run -Werror file.cpp           # check only: non-zero exit if anything would change (what CI runs)
git clang-format                                  # format only the lines you changed
```

The configuration is a YAML file called **`.clang-format`** in the project root (here it is called `format.yaml`). Start from a built-in style and change a few options:

```yaml
BasedOnStyle: LLVM          # or Google, Chromium, Mozilla, WebKit, Microsoft, GNU
IndentWidth: 4
ColumnLimit: 100
BreakBeforeBraces: Allman   # Attach (K&R), Allman, Stroustrup, GNU, Linux ...
PointerAlignment: Left      # int* p   (Right: int *p)
AllowShortFunctionsOnASingleLine: None
```

`clang-format --dump-config` prints every option of the current style (about 150), and `// clang-format off` / `// clang-format on` protect a block such as a hand-aligned table. Run it from the editor on save, from a `pre-commit` hook, and as a **CI check**. Pick a style once, early, and do not discuss it again: *a consistent style matters, which one hardly does.*

### Your first task: infer a style

`style_sample.cpp` (read-only) is formatted in a house style. Edit `format.yaml` until clang-format finds **nothing to change** in the sample. Run Tests lists the lines that clang-format would rewrite: read the sample, find a rule, add the option, run again. (Every option is documented at clang.llvm.org/docs/ClangFormatStyleOptions.html.) You cannot cheat by switching the formatter off (`DisableFormat`) or by removing the line limit (`ColumnLimit: 0` makes clang-format leave line wrapping alone): the Clean tier checks for both.

## CMake: the build system you will meet

C++ has no package manager and no build tool of its own. **CMake** is the de-facto standard *build-system generator*: you describe the project in `CMakeLists.txt`, and CMake writes the real build files (Ninja, Makefiles, Visual Studio, Xcode). Free and open source (BSD).

```
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release      # configure, in a separate build folder ("out-of-source")
cmake --build build                                          # build
ctest --test-dir build --output-on-failure                   # run the tests
```

### Modern CMake is about targets

A **target** (a library or an executable) carries its own properties: its sources, include directories, compile options and the libraries it needs. Commands work on a target, and each property says who sees it:

| keyword | meaning |
|---|---|
| `PRIVATE` | only this target needs it (a warning flag; an internal dependency) |
| `PUBLIC` | this target needs it **and** everything that links to it (a header directory that the public headers use) |
| `INTERFACE` | not this target, only the ones that link to it (a header-only library) |

```cmake
add_library(greeter src/greeter.cpp)                       # list the sources by name
target_include_directories(greeter PUBLIC include)         # whoever links greeter can #include its headers
target_compile_options(greeter PRIVATE -Wall -Wextra)      # warnings for this target only
add_executable(app app/main.cpp)
target_link_libraries(app PRIVATE greeter)                 # app uses greeter (and gets its include path for free)
```

The **old style** (what you find in tutorials from before 2015) used global commands: `include_directories`, `link_libraries`, `add_definitions(-Wall)`, `file(GLOB ...)`, `set(CMAKE_CXX_FLAGS "-std=c++11")`. They affect *every* target in the directory, spread invisibly, and are hard to untangle. Avoid them. Also: `file(GLOB)` for sources is convenient but a new file is not noticed until CMake is re-run, so list the files.

### Tests, packages, and the rest

- **Language standard**: `set(CMAKE_CXX_STANDARD 20)`, `set(CMAKE_CXX_STANDARD_REQUIRED ON)`, `set(CMAKE_CXX_EXTENSIONS OFF)`. Let CMake pick the compiler flag for each compiler.
- **Tests**: `enable_testing()` once, then `add_test(...)`, or with GoogleTest `include(GoogleTest)` and `gtest_discover_tests(my_tests)`, which registers every `TEST` separately. `ctest` runs them (`-R name` filters, `-j8` parallel, `--output-on-failure`).
- **Dependencies**: `find_package(GTest REQUIRED)` uses a library that is installed on the machine. `FetchContent` downloads one at configure time (pin it to a release tag or a commit, never to `main`). Package managers **vcpkg** and **Conan** (both free, open source) install libraries for you and plug into CMake.
- **`CMAKE_EXPORT_COMPILE_COMMANDS ON`** writes `compile_commands.json`, which clang-tidy, clangd and cppcheck need to compile your files like the build does.
- **Sanitizers and options** as a switch: `option(ENABLE_ASAN "Build with AddressSanitizer" OFF)`, then `target_compile_options(... -fsanitize=address)` and `target_link_options(...)`. **Presets** (`CMakePresets.json`) name configurations such as `dev`, `release`, `asan` so that everyone, and CI, uses the same commands.
- **Speed**: Ninja instead of Make, `ccache` to skip recompiling unchanged files, `CMAKE_UNITY_BUILD`, precompiled headers.

### What CI does with all of this

A continuous-integration job (GitHub Actions, GitLab CI, ...) is a script that a server runs on every change:

```
clang-format --dry-run -Werror $(git ls-files '*.cpp' '*.hpp')       # formatting
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Debug -DCMAKE_CXX_FLAGS="-fsanitize=address,undefined"
cmake --build build                                                  # builds with -Wall -Wextra -Werror (from the target options)
ctest --test-dir build --output-on-failure                           # runs the unit tests under the sanitizers
run-clang-tidy -p build                                              # static analysis
```

That is the whole module in six lines: formatter, compiler warnings, unit tests, sanitizers, static analysis.

### Your second task: modernize a `CMakeLists.txt`

`CMakeLists.txt` holds a recipe in the old style for a project with a library, a program and GoogleTest tests (the layout is in the comment at the top; the sources are only imagined). Rewrite it the modern way. (The sandbox has no network and no `cmake`, so **Run Tests checks the structure of the file**, ignoring comments. On your own machine you would run the three commands above.)

- **Works**: the targets exist and are connected (library, program, tests), `enable_testing`, `ctest` knows the tests.
- **Robust**: the standard is a CMake setting; include directories and warnings belong to targets; every link has `PRIVATE`/`PUBLIC`/`INTERFACE`; no legacy global commands and no `file(GLOB)`; `compile_commands.json`; a fetched dependency is pinned.
- **Clean**: `format.yaml` really formats (a base style, the formatter on, a sensible `ColumnLimit`) and clang-format agrees with the sample.
