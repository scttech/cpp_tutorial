# Fast path: what's different in C++

- **A unit test is a function that checks a result and records a failure.** Frameworks add: automatic registration, rich failure messages (file, line, expression, values), isolation, a report, and an exit status. In C++ they are libraries, not part of the language: GoogleTest, Catch2, doctest, Boost.Test.
- **Registration uses static initialization**: a namespace-scope object whose constructor adds the test to a list, before `main`. The list itself must be a **function-local static** (the *construct on first use* idiom), because the order of static initialization across objects and translation units is unspecified (the *static initialization order fiasco*).
- **Check macros need the preprocessor** for `__FILE__`, `__LINE__` and `#expr` (stringizing). Write a multi-statement macro as `do { ... } while (0)`, and evaluate each argument once (`const auto& v = (arg);`).
- **`EXPECT_` vs `ASSERT_`**: `EXPECT_*` records a failure and continues; `ASSERT_*` also leaves the test (`return`, since a macro cannot return a value from the caller otherwise). Use `ASSERT_` when the rest of the test would dereference something that failed to exist.
- **Exit status**: `main` returns non-zero when a test failed, so that CI can fail the build. Exceptions thrown by a test should be caught by the runner and reported as a failure of that test (this lesson's version leaves that out).
- **Test names are documentation**: `a_new_account_has_no_money` says what the code promises; `test1` does not.
