# CPP: From Zero to Hero

[![Pages](https://github.com/scttech/cpp_tutorial/actions/workflows/pages.yml/badge.svg)](https://github.com/scttech/cpp_tutorial/actions/workflows/pages.yml)

An interactive C++ course: write code in the browser, run it against real unit tests, get immediate feedback.
Full product spec: [project_assets/CPP_From_Zero_to_Hero_SPEC.md](project_assets/CPP_From_Zero_to_Hero_SPEC.md).

**[Try the live demo](https://scttech.github.io/cpp_tutorial/)**: the static build, with the compiler running in your browser (no install). Some lessons need the full Docker build and are greyed out there. See [Two ways to run it](#two-ways-to-run-it).

## Status

| Area | State |
|---|---|
| Runner (Docker sandbox + API) | Working, with attack tests |
| Front end (lesson page, editor, tiers, hints) | Working |
| Progress persistence | IndexedDB (localStorage fallback), export/import JSON, migration from the first release, streaks |
| Badges | 63 declarative badges (progress, mastery, skills such as RAII Master and Const Correct, habits such as Refactorer and Good Question, consistency, hidden), Profile page, toasts |
| Friendly compiler errors | 91 rules (14 for template errors, 11 for inheritance and virtual functions, 9 for warnings such as `-Wconversion` and `-Wshadow`, 3 for threads, 4 for `constexpr`/`consteval`/`constinit`), each verified against real Clang output; raw message always one click away |
| Module 1 Foundations | **Complete**: test-out, 7 lessons, capstone (9 items, 51 mutant solutions that must fail) |
| Module 2 Data and Memory | **Complete**: test-out, 7 lessons (arrays, vector, strings, references, pointers and the heap, structs, a Find-the-Bug debug set) and the Inventory Tracker capstone: 9 items, 130 mutant solutions in total that must fail. Uses all four tiers: Fast = counted heap allocations, Clean = leak/double-free detection and no raw `new` |
| Module 3 Classes and Objects | **Complete**: test-out (a normalized `Fraction` with operators), six lessons (constructors/`explicit`, encapsulation and invariants, const correctness with `mutable` caches, operator overloading, `enum class` state machines with `static` members, namespaces/ADL/header hygiene) and the Library Management capstone: 8 items, 244 mutant solutions in total that must fail. Uses the Works, Robust and Clean tiers (Clean scans the header for `using namespace`, code outside the namespace, and raw `new`/`delete`) |
| Module 4 Ownership and RAII | **Complete**: test-out (a `Buffer` that follows the rule of five), five lessons (destructors and RAII, copying and the rule of three, moving and the rule of five, `unique_ptr` and the rule of zero, `shared_ptr` and `weak_ptr`), a debug challenge (six lifetime bugs, each a sanitizer report) and the `IntVector` capstone: 8 items, 219 mutant solutions in total that must fail. Every lesson runs under AddressSanitizer and UBSan with allocation tracking; the Fast tier counts allocations (a move, a swap and a sort must allocate nothing) |
| Module 5 Generic Programming | **Complete**: test-out (a generic `max_by`), six lessons (function templates, class templates, specialization and `if constexpr`, concepts, variadic templates and fold expressions, `auto`/`decltype`/type traits), a debug challenge ("Reading template errors": six different template errors, each with a friendly explanation) and two capstones (`RingBuffer<T, N>` and `StaticVector<T, N>` with manual object lifetimes): 10 items, about 200 mutant solutions in total that must fail. Fast tiers count copies, moves, constructions and heap allocations |
| Module 6 The Standard Library | **Complete**: test-out (Top-K words), seven lessons (containers and their costs, iterators and ranges, algorithms, lambdas and `std::function`, `optional`/`variant`/`span`, `from_chars`/`std::format`/`string_view`, `<chrono>`/`<random>`/numerics) and the Log Analyzer capstone: 9 items, about 250 mutant solutions in total that must fail. The Fast tier uses time limits, allocation counts, comparison counts and copy counts. Four lessons need the full Docker build (they use `<format>`/`from_chars` for floating point, deep recursion or large inputs that the 32-bit browser build cannot match) and are greyed out in the static demo |
| Module 7 Inheritance and Polymorphism | **Complete**: test-out (a polymorphic shape hierarchy with correct virtual destruction), five lessons (inheritance and slicing, virtual functions with `override`/`final` and RTTI, abstract classes and interfaces, composition over inheritance with the fragile base class and the diamond, runtime polymorphism vs `std::variant`/CRTP/type erasure), a debug challenge ("Reading polymorphism bugs": errors, warnings and a silent slicing bug) and the Plugin Pipeline capstone (a registry of interchangeable stages): 8 items, about 350 mutant solutions in total that must fail. The Fast tier of the alternatives lesson compares the four techniques by heap allocations and object sizes. Six of the eight need the full Docker build, because the browser build disables C++ exceptions and these lessons validate their arguments with `throw` |
| Module 8 Testing and Tooling | **Complete**: test-out ("Test the Stack": write GoogleTest tests that must catch nine seeded bugs), nine lessons (why test, with a mini test framework built from macros; GoogleTest; Catch2 and how to choose a framework; test doubles and GoogleMock; compiler warnings as a static analyzer; `clang-tidy` and `cppcheck`; sanitizers, fuzzing and coverage; `clang-format`, CMake and `ctest`), a debug challenge ("Reading test output": GoogleTest failures plus UBSan and ASan reports) and the "Test and Harden a Library" capstone (fix a CSV library, test it so that eight broken copies are caught, and clear warnings, two analyzers and an allocation budget): 11 items, about 150 mutant solutions in total that must fail. Lessons that write tests are graded by **mutation testing**: the learner's tests must pass on the real code and fail on every hidden broken copy. Only the mini-framework and the warnings lessons run in the browser; the rest need the Docker image (GoogleTest, Catch2, clang-tidy, cppcheck and clang-format) |
| Module 9 Concurrency | **Complete**: test-out ("Parallel sum"), six lessons (threads and `std::jthread`; data races and mutexes; condition variables and a blocking queue; atomics and the memory model; futures, `async` and a thread pool; coroutines with a guided `Generator<T>`), a debug challenge ("Reading concurrency bugs": ThreadSanitizer reports, a hang, two `std::terminate`s) and the Job Scheduler capstone (priorities, cancellation, `wait_idle`, drain or discard shutdown): 9 items, about 120 mutant solutions in total that must fail. Every thread lesson is built with `-fsanitize=thread`, so a race or a wrong memory order is a failing test, and every test has its own time limit, so a deadlock is a failing test and not a hung page. Fast tiers count threads and processor time (the sandbox gives a job one CPU, so wall time would say nothing). Only the coroutines lesson runs in the browser; the rest need the Docker runner |
| Module 10 Performance | **Complete**: test-out ("Make it fast": a correct pair counter that must stop being quadratic), six lessons (measuring performance: find the hot stage with per-stage counters, then fix it; memory layout and caches: struct padding, false sharing and array-of-structs to struct-of-arrays, scored by the cache lines a loop touches; allocation costs and arenas: write a `std::pmr::memory_resource` bump allocator; compile-time computation with `constexpr`, `consteval` and `constinit`; a reading guide to `perf`, `perf stat`, Valgrind and Amdahl's law, answered from canned profiles), a debug challenge ("Reading performance problems": six functions that each hide a different cost) and the "Optimize the Pipeline" capstone (a log analyzer that must hit comparison, copy and allocation budgets without changing a result): 8 items, about 130 mutant solutions in total that must fail. Speed is never timed: lessons count comparisons, copies, moves, heap allocations and cache lines with instrumented types, and use scaling tests (twice the input, about twice the cost). All but the debug challenge run in the browser demo |
| Module 11 Hero Projects | **Complete**: six large multi-file projects, each with Works / Robust / Fast / Clean tiers and about 60 mutant solutions that must fail (364 in total): a **JSON Library** (recursive-descent parser with line and column in every error, \u escapes and surrogate pairs, shortest round-trip number output), an **Expression Compiler** (lexer, parser, AST in a `std::variant`, evaluator, and an optimizer that rewrites the tree in place without changing any result or error), a **Mini Database** (query parser, ordered store with a COUNT index, nested transactions with an undo log), a **Text Adventure Engine** (a world data file, item states as a `std::variant`, one polymorphic `Action` per verb, and tests that extend the engine with verbs they bring themselves), a **Tiny Regex Engine** (parser, Thompson NFA, a one-pass leftmost-longest simulation with no backtracking, checked against an independent evaluation of the syntax tree) and a **Memory Allocator** (boundary tags, a free list, coalescing, pointer validation, all bookkeeping inside the heap through a counted accessor). Finish any three for "Zero to Hero"; all six give "Hero of Heroes". Four of them (JSON, database, adventure, regex) run in the browser demo |
| Domain challenge tracks | **Complete**: 49 challenges in eight tracks, each with Works / Robust / Fast / Clean tiers and mutant solutions that must fail (about 1750 in total), listed on the home page after the course. They are recommended after certain modules but never locked, and each track completed at the Works tier earns a badge (Game Dev, Quant, Systems Hacker, Embedded Engineer, Data Wrangler, Protocol Wizard, Graphics Guru, Toolsmith; Domain Polyglot for a challenge in every track). **Games** (6): entity-component system with sparse sets, collision detection with a spatial grid, A* pathfinding, an object pool with generation-checked handles, a fixed-timestep game loop, a variant-based inventory. **Finance** (6): fixed-point money with exact allocation, an order-book matching engine, streaming moving averages, portfolio risk metrics, a zero-copy FIX-style parser, a lock-free SPSC event queue (ThreadSanitizer, server only). **Systems** (7): an LRU cache, a `std::pmr` tracking resource with leak reports, a shell tokenizer, a ring-buffer flight recorder, a consistent-hashing ring, a Bloom filter, a timer wheel. Counting types and differential tests against independent references replace wall-clock timing **Embedded** (6): typed register fields with exact read-modify-write counts, a fixed-capacity vector built on placement construction, a table-driven state machine, CRC with a compile-time table, a button debouncer with click and long-press events, a saturating Q16.16 fixed-point PID controller. **Networking** (6): an incremental, strict HTTP/1.1 request parser, a varint/zigzag/frame binary codec, RFC 3986 URLs with all 40 resolution examples, strict Base64 and hex, token-bucket and sliding-window rate limiters, and DNS messages with name compression and hostile-packet defences. **Data** (6): a strided matrix library with a cache-blocked multiply (checked with a cache model), numerically stable statistics (Welford, compensated sums, percentiles), an RFC 4180 CSV reader and writer, sparse CSR matrices, interpolation and root finding, time-series resampling with overflow-safe int64 time. **Graphics** (6): vector and matrix math, quaternions and slerp, ray intersection, integer rasterization with the top-left rule, a Kalman filter, a pure-pursuit path follower. **Tooling** (6): a command-line argument parser, a config parser with line and column errors, Myers diff with unified patches, a glob matcher that cannot backtrack, a build dependency graph, a structured JSON logger. |
| Sanitizers (Docker runner) | Working: a lesson turns them on with `flags: [..., "-fsanitize=address,undefined"]` and `requires: [sanitizers]`. Reports are symbolized, parsed (kind, file:line, where the memory was freed and allocated) and explained by [content/diagnostics/runtime.yaml](content/diagnostics/runtime.yaml): 18 rules, each verified against real sanitizer output. Module 4 uses them in every lesson |
| Test harness instrumentation | Allocation tracking (leaks, double free, `new[]`/`delete` mismatch, poisoned freed memory) and a copy/move-counting `Tracked` type |
| Static build (in-browser compiler) | Working: [wasm-runner/](wasm-runner/) runs Clang/LLD as WebAssembly in Workers. 80 of 144 lessons run in it ([content/wasm-compat.json](content/wasm-compat.json)); the rest are greyed out. The GitHub Pages workflow ([.github/workflows/pages.yml](.github/workflows/pages.yml)) builds and deploys it |
| Gotchas tab | **Working**, content in progress: each lesson's C++ traps are explained in a `gotchas.yaml` next to it (Modules 1 to 11 so far), and most come with two tiny programs, the trap and the fix, that you can edit and run right in the tab (the sanitizer ones need the Docker version). `tools/validate-content/gotchas-check.mjs` checks every explanation and runs every demo. |
| AI tutor | **Working** (optional, bring your own Anthropic key): a Tutor tab on every lesson with eight modes: Nudge, Explain the concept, Explain my error, Pinpoint, Approach, **Discuss** (a Socratic conversation about the ideas, with no answers), and, once a lesson is passed, Review and Quiz me. It is sent the lesson text, your code, your latest compiler errors and failing-test names, and **never** the reference solution or the hidden tests. Replies in the hint modes are checked locally before you see them (a code block over 12 lines, or one that reproduces the solution, is re-asked once and then replaced by a safe message). Calls go from your browser straight to api.anthropic.com with the key you paste in Settings (kept only in localStorage, never exported), so it works in the static build too. Each question counts as AI help (Self-Reliant); explaining then passing earns Good Question. |

## Two ways to run it

| | **Static build** (GitHub Pages) | **Full build** (docker-compose) |
|---|---|---|
| Compiler | Clang compiled to WebAssembly, in your browser | Real Clang 18 / GCC 14 in a sandbox container |
| Exceptions (`try`/`catch`) | No | Yes |
| Threads / concurrency | No | Yes |
| Sanitizers (ASan/UBSan/TSan) | No | Yes (AddressSanitizer, LeakSanitizer and UBSan; reports explained in plain language) |
| First load | ~90 MB toolchain download (cached) | Nothing to download |
| Lessons needing the server | Shown greyed out, marked "server needed" | Available |

The static build is meant to be deployed to GitHub Pages by [.github/workflows/pages.yml](.github/workflows/pages.yml) on
every push to `master`, so it tracks the docker-compose version. Lessons the browser compiler cannot teach are greyed out and marked
"server needed". Which ones is decided by a compatibility check, not by hand: it runs every lesson's starter, solution
and mutants through the in-browser runner and writes [content/wasm-compat.json](content/wasm-compat.json). The workflow
fails if that file is stale, so a new lesson needs no other change. Both builds share the same UI, lesson format and
`Runner` interface.

Known differences in the browser build: it is 32-bit (`size_t` and pointers are 4 bytes), `std::from_chars` for
floating point is missing, and there are no exceptions, threads or sanitizers. A null-pointer read does not crash.
Completing a module by finishing every lesson is not possible while one of its lessons is greyed out; the test-out
still completes it. All of Module 4 needs the sanitizers, so the static build shows it greyed out.

## Layout

| Path | What |
|---|---|
| `app/` | Vite + React + TypeScript front end |
| `runner-service/` | Node/Fastify API: validates requests, runs each job in a fresh sandbox container |
| `runner-image/` | The sandbox image (Clang, GCC, GoogleTest/GoogleMock, Catch2, doctest, clang-tidy, clang-format, cppcheck, and the in-container job runner) |
| `wasm-runner/` | In-browser runner (Clang/LLD as WebAssembly via browsercc + WASI); also runs in Node for the compatibility check |
| `spike/` | M0 feasibility spike (superseded by `wasm-runner/`) |
| `docker-compose.yml` | Runner service + Docker socket proxy |

## Run the backend

```bash
docker compose --profile build-only build   # builds zth-sandbox and the runner
docker compose up -d runner                 # API on http://localhost:8080
curl localhost:8080/healthz
```

## Build the static (in-browser) version locally

```bash
cd app
VITE_RUNNER=wasm npm run build        # or: VITE_RUNNER=wasm npx vite build --outDir dist-wasm
npx vite preview --port 4173          # serves app/dist; the first visit downloads ~90 MB of compiler
cd ../tools/e2e && node wasm.mjs      # drives it in a browser (APP_URL defaults to http://localhost:4173)
```

## Run the front end

```bash
cd app
npm install
npm run dev          # http://localhost:5173  (expects the runner on http://localhost:8080)
```

Open the course map, pick **Hello, World**, edit the code, and use **Run** (Ctrl+Enter) or **Run Tests** (Ctrl+Shift+Enter).
Set `VITE_RUNNER_URL` to point the app at a runner on a different host.

## Verify everything

```bash
cd app && npm test                                   # 119 unit tests: badge engine, streaks, run recording, import validation, friendly-rule matching
cd tools/validate-content && npm install && npm run validate   # the harness itself (allocation tracking, sanitizers), every lesson (starter fails, solution passes, mutants fail), every friendly rule vs real Clang, every sanitizer rule vs real sanitizer output
cd tools/e2e && npm install && npx playwright install chromium && npm test   # drives the real UI in a browser (16 scripts built on tools/e2e/lib.mjs; wasm.mjs separately, see above)
cd wasm-runner && npm install && npm test            # the in-browser (WASM) runner on small programs; no Docker needed
cd tools/validate-content && npm run compat          # which lessons run in the browser? (incremental; writes content/wasm-compat.json)
```

The validator and e2e need the runner (`docker compose up -d runner`); e2e also needs the dev server. Set `SHOTS=<dir>` to save screenshots.

## Authoring content

- **A badge**: add an entry to a file in [content/badges/](content/badges/). Rules are data (`lesson-count`, `lesson-tier`, `module-complete`, `tier-complete`, `streak-days`, ...), so no code change is needed. Badges whose rules refer to content that does not exist yet appear under "coming soon".
- **A friendly error**: add a rule to [content/diagnostics/friendly.yaml](content/diagnostics/friendly.yaml) with a `trigger` program. `npm run validate` compiles every trigger with real Clang and fails if the rule does not fire, or if an earlier rule shadows it.
- **A lesson**: see [content/tracks/](content/tracks/); every lesson needs a starter, solution, hidden tests, hints and ideally a list of **mutants**: plausible wrong solutions that the tests must reject.
  - **Mutants**: write them in the lesson's `mutants.json` (the source of truth): `{ "name", "why", "edits": [{ "old", "new" }] }`, where `old` must appear exactly once in the solution file. Then run `npm run mutants` in `tools/validate-content` to generate the `mutants/` directories (commit both; CI and `npm run validate` check that they match). Never edit a file in `mutants/` by hand. Changing a solution? Run `npm run mutants` and the validator: an `old` that no longer matches is reported by name.
  - Memory lessons: put `#define ZTH_TRACK_ALLOCATIONS` at the top of `tests/test.cpp`, then use `zth::mem::Scope` with `ZTH_EXPECT_NO_MEMORY_BUGS(scope)` / `ZTH_EXPECT_ALLOCS_AT_MOST(scope, n)`, and `zth::Tracked` to count copies and moves. See [runner-image/include/zth_test.hpp](runner-image/include/zth_test.hpp).
  - A module whose lessons are not all written yet sets `inProgress: true` in its `module.yaml`: finishing the lessons that exist does not complete it or award its Module Master badge (the test-out still can). Remove the flag when the module is finished (Modules 1-4 are).
  - Sanitizer lessons: put `-fsanitize=address,undefined` in the lesson's `flags` and `requires: [sanitizers]` (the static build then greys the lesson out); raise `limits.memoryMb` to 256 (sanitized programs use more memory). A sanitizer report ends only the test it happened in (status "Crashed", `stopped by a sanitizer: ...`); the results panel explains the first report in plain language and offers the raw one. Leaks are reported by LeakSanitizer in plain runs; in tests use `ZTH_TRACK_ALLOCATIONS`, which also poisons freed blocks under AddressSanitizer so a read after `delete` is a report, not a silent garbage value. To add or change an explanation, edit [content/diagnostics/runtime.yaml](content/diagnostics/runtime.yaml) (every rule needs a `trigger` program, checked by `runtime-check.mjs`).
  - Harness helpers for hidden tests (all in [runner-image/include/zth_test.hpp](runner-image/include/zth_test.hpp)): `ZTH_EXPECT_THROWS(Ex, code)` / `ZTH_EXPECT_NO_THROW(code)` (the failure message quotes the code and what was thrown instead), `zth::Lcg` (a seeded random generator, so a failing random test is reproducible), and `zth::source_layout_of("file.hpp")` (what is at file scope: namespaces, `using namespace`, leftovers).
  - **Test frameworks and analyzers** (Docker runner only; the lesson says `requires: [testing-tools]`): `libs: [gtest, gmock, catch2, doctest]` links a framework (its `main` is provided); `analyzers: [{ tool: clang-tidy | cppcheck | clang-format, files: [...], checks: "...", config: tidy.yaml }]` runs after the build, turns every finding into a Problems entry and adds one "clean"-tier result per tool (so a lesson can ask for "no findings"); a config file the learner may not change is listed under `files.readonly` and called `tidy.yaml` or `format.yaml` (the loader skips dotfiles; the lesson text says the real name).
  - **Lessons that grade the learner's tests** (`grader: variants`): the learner writes the tests, the lesson ships the code under test. `tests/grade.json` lists hidden **variants**, each a copy of the code with one bug in `tests/variants/<name>/<file>`. Run Tests builds the learner's tests against the real code (they must pass) and against every variant (they must fail: a survivor is reported by name, with its `hint`). `"check": "check.cpp"` adds a hidden harness program that tests the learner's own library (and may read the learner's test file to ask for fixtures, generators, matchers...), `"testSources": [...]` names the learner's test files, `"clean": true` adds "compiles without warnings". Weak test suites go in `mutants.json` as usual: each must fail something. Keep the number of variants near eight: every one is a compile (about 2 s each in the sandbox).
  - **Thread lessons**: `flags: [..., "-pthread", "-fsanitize=thread"]` and `requires: [threads, sanitizers]`. ThreadSanitizer reports a data race, a wrong memory order or a lock-order inversion deterministically (it needs both accesses to happen, not to overlap), and a deadlock ends as a test timeout. In `tests/test.cpp` use `ZTH_TEST_TIMEOUT(tier, name, seconds)` (the default limit is 2 s per test; keep every limit well under the lesson's `wallMs`, because a run that the time limit kills reports only the tests that had finished, and the app and the validators then treat the whole run as failed), start threads together with a `std::latch`, force an interleaving with a latch or a "stalling" wrapper instead of a sleep, and measure waiting with `zth::cpu_ms()` (processor time) rather than wall time: the sandbox gives a job one CPU. A TSan report is parsed into the results card (`previousAt` is the other access); its friendly explanations live in [content/diagnostics/runtime.yaml](content/diagnostics/runtime.yaml).
  - More harness helpers for hidden tests: `ZTH_EXPECT_NO_RAW_MEMORY("main.cpp")` (scans the learner's source for `new`/`delete`/`malloc`/`free`), `zth::dirty_stack()` (makes uninitialized reads reproducible), and placement-new over `0xAB`-filled memory to expose member-initialization-order bugs.

Backend tests (need Docker):

```bash
cd runner-service
npm test                    # fast unit tests
npm run test:integration    # "attack" tests: fork bomb, memory hog, huge output, network, read-only fs...
```

## Security model

Untrusted code is compiled and run inside a **new container per job**, with: no network, read-only root filesystem,
size-capped tmpfs for `/work` and `/tmp`, all Linux capabilities dropped, `no-new-privileges`, non-root user (10001),
PID / memory / CPU limits, no swap, a wall-clock timeout and an output cap. The container is always removed afterwards.
The API additionally allow-lists compiler flags, validates file names and sizes, clamps limits to server ceilings,
rate-limits clients and bounds the job queue.

**Residual risks (read before exposing this to anyone you don't trust):**
- The runner talks to Docker through `tecnativa/docker-socket-proxy`, which filters by URL path only. Because the
  runner must be allowed to create containers, a *compromised runner service* could ask Docker for a privileged
  container, which is host-root equivalent. The runner is hardened (read-only, no capabilities, validated input), but
  this is the weakest link. Mitigations for a real deployment: rootless Docker, Sysbox, or an authorization plugin.
- Standard containers share the host kernel. For stronger isolation of the *sandboxes*, set `SANDBOX_RUNTIME=runsc`
  to run them under [gVisor](https://gvisor.dev) (Linux hosts; not tested on Docker Desktop for Windows).
- This is designed for local / trusted use. Do not put it on the public internet as-is.
