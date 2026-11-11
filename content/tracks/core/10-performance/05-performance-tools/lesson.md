# Performance tools (a reading guide)

> **You have a rule (measure first) and some counters. Real programs need real tools.** This lesson is a map of the tools you will meet, what each one is good for, and how to read what it prints. Sanitizers, `clang-tidy`, `clang-format`, CMake and test frameworks were Module 8; this lesson is only about *speed*.

## 1. Build the right thing first

- **Optimize before you measure.** Use `-O2` (or `-O3`/`-O2 -march=native` if you know the target). An `-O0` build keeps every temporary, never inlines, and has hot spots the optimized program does not have.
- **Keep debug info and frame pointers**, so the profiler can name functions and lines: `-O2 -g -fno-omit-frame-pointer`. (`-g` does not slow the program down; it only makes the file bigger.)
- **Measure the real workload**, on a quiet machine, several times. One timing means little; look at the spread.

## 2. Sampling profilers: where does the time go?

A **sampling profiler** interrupts the program hundreds of times per second and records which function is running (and who called it). Slow functions show up in many samples. The overhead is small, so you can profile the real program.

| Tool | Platform | Notes |
|---|---|---|
| `perf` | Linux | `perf record -g ./prog`, then `perf report` (a terminal UI) or a flame graph |
| Instruments (Time Profiler) | macOS | in Xcode |
| Visual Studio Profiler, VTune | Windows (VTune also Linux) | VTune is free, from Intel |
| gprof, `-pg` | everywhere | old and coarse; mostly historical |

**Self time vs inclusive time** is the first thing to learn to read. *Self* is the time in the function's own instructions. *Inclusive* (perf calls it "Children") is the function plus everything it called. `main` always has a huge inclusive time and a tiny self time: it tells you nothing. To find where to work, look at **self** time; to see which *part of the program* is the expensive one, look at **inclusive** time. A **flame graph** draws inclusive time as stacked boxes (the wider the box, the more time) and makes this visible at once.

## 3. Hardware counters: why is it slow?

`perf stat` asks the processor's own counters. Three numbers answer "what kind of slow is this?":

- **Instructions per cycle (IPC).** A core can finish 2 to 4 instructions per cycle when it is fed. An IPC below 1 means it spends most of its time *waiting*.
- **Cache-miss rate** (`cache-misses` / `cache-references`). High (10-20 % and more) plus a low IPC means *memory-bound*: fix the layout and the access pattern (the previous lessons), not the arithmetic.
- **Branch-miss rate.** Above 5 % the processor is guessing wrong about `if`s (random data in a branchy loop): consider branch-free code or sorting the data first.

## 4. Counting tools: exactly how much, how often?

**Valgrind** runs the program on a simulated processor (50 times slower, but exact and repeatable, with no noise).

- `--tool=callgrind` counts instructions and calls per function and who called whom; view it with `callgrind_annotate` or KCachegrind. **Call counts matter:** a tiny function called 48 million times can be cheap each time and still worth inlining, while a function called 12 000 times can be the real cost.
- `--tool=cachegrind` simulates the caches (miss counts per line).
- `--tool=massif` is a heap profiler (who allocates how much, over time). `heaptrack` is a faster alternative on Linux.

This is the same idea as the counters on this site, applied to a real program.

## 5. Other tools worth knowing

- **Compiler Explorer (godbolt.org)** shows the assembly your code becomes, with any compiler and flags: the way to answer "did the compiler really inline / vectorize this?".
- **Micro-benchmarks**: Google Benchmark (or `std::chrono::steady_clock` with care) for comparing two versions of one function. Prevent the optimizer from deleting the work (`benchmark::DoNotOptimize`), and warm up.
- **`ccache`** caches compiler output, **`-ftime-trace`** (Clang) shows where *compile* time goes, and **`ninja`** builds in parallel: build speed matters to you, even if not to users.
- **`std::pmr` and counting allocators**, as in the previous lesson, for allocation behaviour; **sanitizers** (Module 8) for bugs, not speed.

## 6. Amdahl's law

If a part of the run that takes a fraction `p` of the time becomes `s` times faster, the whole run takes `(1 - p) + p / s` of the old time:

```
speedup = 1 / ((1 - p) + p / s)
```

If `p` is 61 % and `s` is 10, the speedup is `1 / (0.39 + 0.061) = 2.2`, and even an infinitely fast `blur_row` could not do better than `1 / 0.39 = 2.6`. **The ceiling is set by everything you did not touch**, so work on the biggest *share* first, and stop when the next share is too small to matter.

## The exercise

Three canned outputs of the same program (`perf-report.txt`, `perf-stat.txt`, `callgrind.txt`) sit next to `answers.hpp`. They are real formats; read the comment at the top of each file for the columns. Answer the six questions in `answers.hpp` and press **Run** to see your answers printed.

## The tiers

- **Works**: self time, inclusive time, and the kind of bottleneck.
- **Robust**: call counts versus cost, Amdahl's law, and why an unoptimized build misleads.
