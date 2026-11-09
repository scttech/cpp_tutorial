# Debug challenge: Reading test output

A failing test is a message from the past: someone (you, yesterday) wrote down exactly what should happen, and today it does not. The skill this challenge trains is **reading that message** quickly and correctly. In this module you wrote tests; now you read what other people's tests, and the sanitizers, say.

## The setup

`bank.hpp` (the code you edit) is a small `Account` class. `bank_test.cpp` (read-only) is its GoogleTest suite. The code has **six bugs**. Press **Run**: the program is the test suite, and the Output tab shows its report. Read it, fix one bug in `bank.hpp`, run again. **Never edit the tests**: a test that fails is correct until proven otherwise.

## How to read the output

**GoogleTest** prints one block per failure:

```
bank_test.cpp:35: Failure
Expected equality of these values:
  a.balance()
    Which is: 500
  0
[  FAILED  ] Account.WithdrawingEverythingLeavesZero (0 ms)
```

1. **The line** `bank_test.cpp:35`: where the test *checked* something that was wrong. Look at it: what was the test trying to establish?
2. **"Which is"**: the *actual* value; the line above it names the expression. The other value is the *expected* one. The difference alone often tells the story: the balance is still 500, so the withdrawal that the test just made did not happen (look at the failure just above it: it says why).
3. **For exceptions** the message is different: *"Expected: a.deposit(-5) throws an exception of type std::invalid_argument. Actual: it throws a different exception type."* or *"it throws nothing"*. Read the *type* names: `std::logic_error` is the parent of `std::invalid_argument`, not the same type.
4. **The summary** at the end lists every failed test. All the tests run, one failure does not hide the others.

**Sanitizer reports** are different: the program **stops** at the first one, so the tests after it never run. This build uses `-fsanitize=address,undefined`.

```
bank.hpp:36:20: runtime error: signed integer overflow: 2147483647 + 1 cannot be represented in type 'int'
```
*UndefinedBehaviorSanitizer*: the `file:line:column` is the line of the bad operation, and the sentence says which rule was broken. Here an `int` was added past its largest value, which is undefined behavior in C++ (it is not "wrapping around").

```
ERROR: AddressSanitizer: heap-use-after-free on address 0x...
READ of size 8 at 0x... thread T0
    #0 0x... in std::string::compare ...
    #3 0x... in Account_TheLastTransaction...::TestBody() bank_test.cpp:60
0x... is located 0 bytes inside of 64-byte region [...)
freed by thread T0 here:
    #1 ... in std::vector<Transaction>::_M_realloc_insert ... bank.hpp:44
previously allocated by thread T0 here:
```
*AddressSanitizer*: three stacks tell a story: **where it was used** (the first one that mentions your code), **where it was freed** (here: the vector grew and moved its elements), and **where it was allocated**. Read them top to bottom; look for the first frame in a file of yours.

## Three habits

1. **Fix the first problem first.** A sanitizer stops the program; an earlier bug may cause a later failure (a corrupted balance makes the next test fail too). After each fix, run again and re-read.
2. **Understand before you change.** A change that makes the message go away is not necessarily a fix. Ask why it failed, and what the code should do (the comment above the class says).
3. **The test is the specification.** If you believe the test is wrong, check the comment at the top of `bank.hpp` first. Here it is right.

## Your task

Make the whole suite pass, and the program run without a sanitizer report. **Run Tests** then runs the hidden checks:

- **Works**: the rules of the account: deposits, withdrawals (including exactly the balance), the exception *types*, a transfer, the ids.
- **Robust**: the overflow, a frozen account, the history, a transaction read earlier staying valid while the account grows, and the all-or-nothing transfer (it must hold when the *first* step fails, when the *second* does, and when the receiver would overflow).

Six bugs: you will meet them as *wrong value*, *wrong exception type*, *undefined behavior*, *use after free*, *money lost*, and *two things with the same id*.
