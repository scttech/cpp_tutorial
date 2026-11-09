# Why test, and what a test is

Every program you have written so far was checked the same way: you ran it and looked at the output. That works for ten lines. It does not work for ten thousand, and it does not work the day after, when you change something and have to look at *everything* again.

A **unit test** is a small program that runs a piece of your code and checks the result **for you**, so that the checking costs nothing to repeat. A suite of them is a safety net: change the code, run the tests, and know in a second whether you broke something.

## The anatomy of a test

Every test, in every framework, has three parts (*arrange, act, assert*):

```cpp
MT_TEST(a_new_account_has_no_money) {
    Account account;                       // arrange: build what you need
    long balance = account.balance();      // act:     do the one thing under test
    MT_CHECK_EQ(balance, 0);               // assert:  compare the result with what you expect
}
```

Good tests are **small** (one behavior each), **independent** (they can run in any order; one test never relies on another), **fast**, and they say **what went wrong** when they fail: not just "failed", but "expected 4, got 3, at line 23".

## What a framework adds

A test framework is mostly bookkeeping:

1. a **registry**: a list of all the tests, so that you do not have to write the `main` that calls each one;
2. **checks** that, on failure, *record* the failure (with file, line, the expression and the values) and let the test continue;
3. a **runner** that runs every test, isolates their results from each other, prints a report, and returns an **exit status** so that a build server can tell pass from fail.

GoogleTest and Catch2 do exactly this, with much more polish. In this lesson you build the core yourself, in about 80 lines, to see that there is no magic in it. It also practices several things from earlier modules: `std::function`, function-local statics, macros, `std::ostringstream`.

## Your task: `minitest.hpp`

The comment at the top of the starter file lists the functions, the macros and the exact format of the report. Fill in the four TODOs. `main.cpp` (read-only) uses your framework on a small `is_leap_year` function; one of its tests is wrong on purpose, so that you can see a failure report. Press **Run**.

### Three ideas worth understanding

**The registry must exist before `main`.** `MT_TEST` creates a `static` object whose constructor calls `mt::add(...)`. That happens during *static initialization*, before `main()`, and the order between different objects (and different files) is not defined. If the list of tests were a plain global variable, `add` could run *before the list is constructed*: the *static initialization order fiasco*. The fix is to keep the list in a **function-local `static`**: it is constructed the first time the function is called, whoever calls first.

**Macros are the only way to see the source text.** `__FILE__`, `__LINE__` and `#cond` (the *stringizing* operator: it turns the argument's source text into a string literal) only exist in the preprocessor. That is why `MT_CHECK` is a macro and not a function. Macros have sharp edges, and two rules cover most of them:

- Wrap a multi-statement macro in `do { ... } while (0)`. Then `if (ready) MT_CHECK(x); else ...` is still one statement; without it the `;` and the `else` break the program.
- Evaluate every argument **once**. A macro that pastes `(a)` into its text twice runs `next()` twice for `MT_CHECK_EQ(next(), 3)`. Copy the argument into a variable first: `const auto& left = (a);`.

**A check records, it does not stop.** `MT_CHECK` is like GoogleTest's `EXPECT_*`: the test goes on, so one run reports *all* the problems. (The stopping kind, `ASSERT_*`, needs a `return` or an exception.)

### The tiers

- **Works**: registering, running, the report, the return value, and the three macros.
- **Robust**: several failures in one test, isolation between tests, arguments evaluated once, values of any printable type, running twice.
- **Clean**: no raw `new`/`delete`, no `using namespace` in the header, and the macros wrapped in `do { } while (0)`.
