# Sanitizers, fuzzing and coverage

A test checks the cases **you** thought of. The worst bugs live in the cases you did not. Three tools attack that problem from different sides: **sanitizers** make hidden bugs visible when the program runs, **fuzzing** generates the unexpected inputs for you, and **coverage** tells you which parts of the code your tests never touched.

## Sanitizers: make the invisible bug loud

You have used these since Module 2. A sanitizer is a compiler option (`-fsanitize=...`) that adds checks to your program; when one fails, the program stops with a report that names the bug and the line. The program runs 2 to 10 times slower, which is fine for tests.

| flag | finds |
|---|---|
| `-fsanitize=address` (**ASan**) | out-of-bounds reads and writes on the heap, the stack and globals; use after free; double free; (with `detect_leaks=1`) memory leaks |
| `-fsanitize=undefined` (**UBSan**) | signed integer overflow, shifts out of range, null pointer use, misaligned access, a bad `enum` or `bool` value, a function that falls off its end |
| `-fsanitize=thread` (**TSan**) | data races between threads (Module 9). It cannot be combined with ASan |
| `-fsanitize=memory` (**MSan**) | reads of uninitialized memory (Clang on Linux, and the whole program including the standard library must be instrumented, so it is the hardest to use) |

Add `-g -fno-omit-frame-pointer` for readable stack traces. Build the tests with ASan and UBSan **always** in CI. A sanitizer only reports a bug that **runs**: it needs a test or an input that reaches it. That is where fuzzing comes in.

## Fuzzing: let the computer invent the inputs

A **fuzz test** calls a function with a huge number of generated inputs and checks that it never misbehaves: no crash, no hang, no sanitizer report, and (if you can state one) no violated **property**. It is especially good for code that parses *untrusted* input: file formats, network protocols, command lines, anything that came from outside. Many serious security bugs in C and C++ were first found by a fuzzer.

Kinds of fuzzing, from simple to powerful:

1. **Random bytes.** Cheap and finds the shallow bugs. But most random inputs are rejected at the first check ("the first byte is not the magic number"), so they never reach the interesting code.
2. **Structure-aware generation.** Build inputs that are *valid* (here: with `make_packet`) and vary them: empty payloads, huge payloads, many packets. This reaches deep code.
3. **Mutation.** Take a valid input and damage it: **cut it short** (every prefix!), flip a byte, repeat a part, change a length field. Most real bugs live right at the edge of the valid.
4. **Coverage-guided fuzzing** (**libFuzzer**, built into Clang; **AFL++**; honggfuzz). The fuzzer watches which code each input reaches and keeps the inputs that reach something new, mutating them further. It discovers `A5 00 00` by itself. You write one function and the engine does the rest:

```cpp
extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t* data, std::size_t size) {
    parse(data, size);          // the sanitizers are the oracle
    return 0;
}
// clang++ -g -fsanitize=fuzzer,address,undefined fuzz.cpp -o fuzz && ./fuzz corpus/
```

Google's **OSS-Fuzz** runs libFuzzer, AFL++ and others continuously on hundreds of open-source projects. This course's runner does not include the libFuzzer engine, so you write the generators by hand: that teaches what the engine automates.

### Two details that matter

**The sanitizer needs an exactly sized buffer.** ASan puts a forbidden "red zone" *after* each heap block. If `parse` reads one byte past the end of its input, ASan notices only if the input's buffer ends exactly there. A buffer that is larger than the input (a reused `std::array<std::uint8_t, 4096>`, a vector with spare capacity) lets the bad read land on harmless bytes. Copy each input into a fresh `std::vector` (a copy has exactly `size()` elements) before you call the code.

**A crash is not the only bug.** Some bugs return a *wrong answer* and never crash. A **property** checks them: "for every list of payloads, `parse(encode(x))` gives back `x`" is a **round-trip property**, and it finds a parser that loses a packet. Other classic properties: sorting twice is sorting once; `decode(encode(x)) == x`; the output of an optimized function equals the output of a simple, slow one. A library for this style is called *property-based testing* (RapidCheck in C++, Hypothesis in Python).

Always use a **fixed seed**: a failure that you cannot reproduce cannot be fixed. (A real fuzzer saves the failing input to a file for the same reason.)

## Coverage: what the tests never touched

**Code coverage** records which lines (or branches, or functions) ran during the tests. With Clang: `-fprofile-instr-generate -fcoverage-mapping`, then `llvm-profdata merge` and `llvm-cov show`; with GCC: `--coverage` and `gcov` (or `lcov`/`gcovr` for HTML reports). The report marks each line with the number of times it ran, and the lines with `0` are code that no test ever reached.

```
   12|      5|  if (size - pos < 3) return result;     <- ran 5 times
   13|      0|  ...                                    <- NEVER ran: write a test for it
```

Use coverage to find the **gaps**. Do not use it as the **goal**: 100% line coverage means every line ran, not that anything *checked* the result. This function has 100% coverage from the single test below, and still has a bug:

```cpp
int abs_diff(int a, int b) { return a > b ? a - b : a - b; }   // the second branch should be b - a
TEST(Diff, Works) { EXPECT_EQ(abs_diff(5, 3), 2); abs_diff(3, 5); }   // both branches run; only one result is checked
```

The honest measure is the one you have been using in this module: **mutation testing**. Change the code on purpose (the graders do) and see whether a test fails. Tools for real projects: *Mull* and *dextool* for C++. A suite that lets mutants survive is a suite with gaps that coverage cannot show.

## Your task

`packet.hpp` (read-only) holds a small binary protocol (`A5`, a length byte, the payload, a checksum) and its `parse` function, correct. Write `fuzz_test.cpp`: a program whose `main` fuzzes `parse`. **Run Tests** builds it with ASan and UBSan and runs it against the real parser, and then against six **broken parsers**, each with one realistic bug. Your fuzzing must make each of them crash, hang, or trip one of your own checks.

- **Works**: it runs cleanly on the real parser, and finds the three memory errors (reads past the end of the input in three different places).
- **Robust**: it also finds a parser that rejects long payloads, one that hangs on an empty packet, and one that silently loses a packet.
- **Clean**: deterministic (fixed seed, no clock), uses `make_packet` to build valid inputs, and checks a property and stops the program when it fails.

A bug that your fuzzing misses is listed by name, with a hint about the kind of input you did not try.
