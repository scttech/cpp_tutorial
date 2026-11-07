# CPP: From Zero to Hero — Project Spec & Curriculum

> **How to use this document:** This is a product spec, architecture plan, full curriculum outline, and badge system for an interactive C++ learning website. It is written to be handed to Claude Code (or any developer) as the source of truth. Start with **Section 10 (Roadmap)** to see the build order, and use Sections 3–4 as the contracts for content and the test runner.

---

## 1. Vision & Goals

**Name:** CPP: From Zero to Hero

**One-liner:** A hands-on C++ course where you write code in an in-browser editor, run it against real unit tests, and get immediate pass/fail feedback, with an optional AI tutor that guides you without giving away answers.

**Primary user:** An experienced programmer (comfortable in other languages) who wants to learn C++ properly, *including* the beginner material, but without being bored by it.

**Goals**
1. Learn by building: every lesson is a small project with requirements and automated tests.
2. Teach what is *different and dangerous* about C++ (undefined behavior, ownership, lifetimes), not just syntax.
3. Offer realistic, domain-flavored challenges (games, finance, embedded, systems, etc.).
4. Make correctness only the first bar: reward robustness, performance, and clean idioms too.
5. Gamify lightly with verifiable badges.
6. Optional "bring your own Claude key" tutor that nudges, not solves.

**Non-goals (v1)**
- User accounts, servers, or a database (designed so they can be added later).
- Multiplayer / social features.
- Supporting every C++ toolchain or platform (we target one standard: C++20, with a path to C++23).

---

## 2. Architecture

### 2.1 Decision: static-first, backend-ready

The site is a **fully static web app** (HTML/JS/WASM assets). C++ is compiled and run **inside the browser** using a Clang toolchain compiled to WebAssembly. No server is needed to host it.

| Concern | v1 (static) | Later (optional backend) |
|---|---|---|
| Compile + run | Clang/LLVM in WASM, inside a Web Worker | Sandboxed containers (Docker/gVisor, or Judge0/Piston) |
| Hosting | Any static host (GitHub Pages, Netlify, Cloudflare Pages) | App server + DB |
| Progress | `IndexedDB` / `localStorage` (+ JSON export/import) | Accounts, sync |
| AI tutor | Browser calls Anthropic API directly with user's own key | Server proxy so no key is needed |
| Badges | Evaluated client-side from stored results | Stored server-side, shareable profiles |

### 2.2 The Runner interface (the key abstraction)

All compile/run/test logic sits behind one interface so the WASM backend can later be swapped for a server backend **without touching lessons or UI**.

```ts
interface RunRequest {
  userFiles: Record<string, string>;     // e.g. { "main.cpp": "...", "solution.hpp": "..." }
  testFiles: Record<string, string>;     // hidden tests + harness header for this lesson
  flags: string[];                       // e.g. ["-std=c++20", "-Wall", "-Wextra"]
  stdin?: string;
  limits: { wallMs: number; memoryMb: number; maxOutputBytes: number };
}

interface CompileDiagnostic {
  severity: "error" | "warning" | "note";
  file: string; line: number; column: number;
  message: string;               // raw compiler message
  friendly?: string;             // our plain-language rewrite, if matched
}

interface TestResult {
  name: string;                  // e.g. "handles empty string"
  status: "pass" | "fail" | "error" | "timeout";
  message?: string;              // e.g. "expected 'cba', got 'abc'"
  durationMs?: number;
  tier?: "works" | "robust" | "fast" | "clean";
}

interface RunResult {
  compiled: boolean;
  diagnostics: CompileDiagnostic[];
  stdout: string;
  stderr: string;
  tests: TestResult[];           // empty if lesson has no tests
  exitCode: number | null;
  timedOut: boolean;
  metrics?: Record<string, number>;   // e.g. op counts, allocations, peak bytes
}

interface Runner {
  run(req: RunRequest): Promise<RunResult>;
  cancel(): void;
  warmUp(): Promise<void>;       // preload toolchain
}
```

**Implementation notes**
- Run the toolchain in a **Web Worker**; enforce the wall-clock limit by **terminating the worker** (handles infinite loops). Respawn on next run.
- Cache the toolchain assets (tens of MB) with a **Service Worker** + `Cache Storage` so only the first visit is slow. Show a clear one-time "Downloading C++ toolchain (cached after this)" progress bar.
- Evaluate existing WASM-Clang options (e.g. the approach used by `binji/wasm-clang`, WASI-based clang builds) and pick one that supports the C++20 standard library headers we need. **Spike this first** (see Roadmap M0); it is the largest technical risk.
- Compile with `-std=c++20 -Wall -Wextra` by default; lessons may override.
- Capture compiler output and parse into `CompileDiagnostic[]`.

### 2.3 Suggested tech stack

- **Frontend:** TypeScript, Vite, React (or Svelte; developer's choice), Tailwind or CSS modules.
- **Editor:** Monaco Editor (VS Code's editor) or CodeMirror 6 with C++ highlighting; inline diagnostics (squiggles) from `CompileDiagnostic`.
- **Content:** Markdown + YAML frontmatter, loaded at build time (e.g. Vite glob import or MDX).
- **State:** a small store (Zustand or Svelte stores) persisting to IndexedDB (e.g. via `idb`).
- **Testing:** Vitest for UI/logic; the C++ lesson tests themselves are validated by a CI script (see 3.4).
- **Hosting:** static.

### 2.4 Persistence model (client-side)

```ts
interface Progress {
  version: 1;
  lessons: Record<LessonId, {
    status: "locked" | "available" | "in_progress" | "complete";
    bestTiers: { works: boolean; robust: boolean; fast: boolean; clean: boolean };
    attempts: number;          // number of Run presses
    runsToFirstPass?: number;
    hintsUsed: number;         // reveals of the hint ladder (static + AI)
    aiHelpUsed: number;
    solutionRevealed: boolean;
    testedOut?: boolean;
    lastCode: Record<string, string>;
    firstCompleteAt?: string;  // ISO
  }>;
  streak: { current: number; longest: number; lastActiveDate: string };
  badges: Record<BadgeId, { earnedAt: string; evidence?: Record<string, unknown> }>;
  settings: { pace: "full" | "fast"; theme: string; apiKeySet: boolean };
}
```

- **Export/Import** as a JSON file from Settings so users never lose progress.
- Never store the Anthropic API key anywhere except `localStorage` under a clearly named key, never in the progress export.

---

## 3. Content Format

### 3.1 Directory layout

```
content/
  tracks/
    core/
      01-foundations/
        module.yaml                  # module metadata, test-out challenge ref
        01-hello-world/
          lesson.md                  # lesson text (full path)
          fast.md                    # "coming from other languages" summary (fast path)
          starter/main.cpp           # what the user sees in the editor
          tests/test.cpp             # hidden tests (compiled with user code)
          solution/main.cpp          # reference solution (NEVER sent to the AI tutor)
          hints.yaml                 # static hint ladder
          lesson.yaml                # metadata (below)
    domains/
      games/
        challenges/
          ecs/ ...                   # same shape as a lesson
```

### 3.2 `lesson.yaml`

```yaml
id: core.foundations.hello-world
title: "Hello, World (and what just happened)"
module: core.foundations
type: lesson            # lesson | challenge | capstone | test-out | debug
estimatedMinutes: 10
concepts: [main-function, iostream, compilation-model]
prerequisites: []
files:
  editable: [main.cpp]
  readonly: []
flags: ["-std=c++20", "-Wall", "-Wextra"]
limits: { wallMs: 5000, memoryMb: 128 }
tiers:                   # which tiers this lesson supports
  works: true
  robust: false
  fast: false
  clean: false
gotchas: [undefined-behavior-intro]   # ids shown in the "C++ gotchas" panel
```

### 3.3 `hints.yaml` (static hint ladder)

```yaml
ladder:
  - level: 1   # nudge
    text: "What does the compiler say about line 5? Read the first error only."
  - level: 2   # concept
    text: "Remember that a C++ program starts at `main`, which must return `int`."
  - level: 3   # pinpoint
    text: "Check the return type of `main` and the semicolon after the `cout` statement."
  - level: 4   # approach (pseudocode, no final code)
    text: |
      1. Include <iostream>
      2. Define int main()
      3. Print the greeting using std::cout
      4. Return 0
```

### 3.4 Lesson validation in CI

A script must verify that, for **every** lesson:
1. `starter/` compiles (or is intentionally broken, flagged in `lesson.yaml`) and **fails** at least one test.
2. `solution/` compiles with **zero warnings** and passes **all** tests for all tiers the lesson declares.
3. Tests run within limits on the WASM runner (use headless Node or Playwright).
4. Every test has a human-readable name and failure message.

This keeps the content trustworthy as it grows.

---

## 4. Test Harness Spec

### 4.1 Harness header

Ship a tiny single-header test framework (`zth_test.hpp`) injected into every test build. It should avoid heavy dependencies; a thin custom harness is preferable to a full framework, to keep compile times low in WASM.

Example usage in a lesson's `tests/test.cpp`:

```cpp
#include "zth_test.hpp"
#include "solution.hpp"   // user's header, or user's main.cpp is compiled in

ZTH_TEST("works", "reverses a normal string") {
    ZTH_EXPECT_EQ(reverse_string("abc"), "cba");
}
ZTH_TEST("robust", "handles empty string") {
    ZTH_EXPECT_EQ(reverse_string(""), "");
}
ZTH_TEST("fast", "linear time on 1e6 chars", ZTH_BUDGET_OPS(5'000'000)) {
    std::string big(1'000'000, 'x');
    ZTH_EXPECT_EQ(reverse_string(big).size(), big.size());
}
```

### 4.2 Output protocol

The harness prints one JSON object per line to **stdout** with a reserved prefix, so user `cout` output and test results never collide:

```
@@ZTH {"event":"test","name":"reverses a normal string","tier":"works","status":"pass","ms":0.3}
@@ZTH {"event":"test","name":"handles empty string","tier":"robust","status":"fail","msg":"expected \"\", got \"x\""}
@@ZTH {"event":"summary","passed":1,"failed":1}
```

The runner strips `@@ZTH` lines out of the user-visible stdout and parses them into `TestResult[]`. Crashes (signal/trap) and timeouts are reported as `error`/`timeout`.

### 4.3 Tier definitions

| Tier | Meaning | How it's checked |
|---|---|---|
| **Works** | Meets the functional requirements on normal inputs | Standard unit tests |
| **Robust** | Handles edge cases and bad input without UB or crashes | Edge-case tests; no traps; optional `-fsanitize` where supported by the WASM build |
| **Fast** | Meets a performance expectation | **Do not use raw wall-clock time** (noisy in browsers). Use deterministic measures: operation/allocation counters via instrumented types, or **scaling tests** (time at n vs 2n or ops count), or step limits |
| **Clean** | Idiomatic and safe style | Compile with `-Wall -Wextra -Werror`, plus lightweight source checks (regex/AST-lite): e.g. no raw `new`/`delete`, no C-style casts, no `using namespace std;` in headers, `const` correctness where required |

A lesson declares which tiers it supports. A challenge badge for a tier is earned only when **all** tests of that tier pass.

### 4.4 Friendly compiler errors

Maintain a rule table in `content/diagnostics/friendly.yaml` mapping regexes on Clang messages to plain-language explanations with a link to the relevant lesson:

```yaml
- match: "use of undeclared identifier '(\\w+)'"
  friendly: "The compiler doesn't know the name `$1` yet. Did you forget an #include, a declaration, or a `std::` prefix?"
  lesson: core.foundations.includes-and-namespaces
- match: "no matching function for call to '(\\w+)'"
  friendly: "None of the overloads of `$1` accept the arguments you passed. Compare your argument types to the function signature."
```

Always show the raw message too (expandable). Start with ~30 of the most common errors and grow over time.

---

## 5. Curriculum

**Target standard:** C++20. **Pacing:** Each module has a **Full path** (gentle) and a **Fast path** (a one-screen "what's different in C++" summary for experienced programmers). Every module begins with an optional **Test-Out Challenge**: pass it to mark the module complete (and earn its badge), but lessons remain accessible.

Each lesson below lists: *Goal*, *Exercise*, and *Gotchas* (C++-specific traps that experienced developers hit).

### Module 1 — Foundations
*Test-out:* "Temperature table": read a range from stdin and print a formatted Celsius/Fahrenheit table, including correct handling of integer vs floating-point division.

1. **Hello, World (and what just happened)** — Goal: `main`, `#include`, `std::cout`, compile → link → run mental model. Exercise: print a greeting, then fix a deliberately broken program. Gotchas: header vs source, `main` returns `int` implicitly.
2. **Variables and fundamental types** — Goal: `int`, `double`, `char`, `bool`, `auto`, `const`, `constexpr`, brace initialization. Exercise: unit-conversion helper functions. Gotchas: **uninitialized variables are UB**, narrowing conversions, `char` signedness, `int` size not fixed.
3. **Operators and integer behavior** — Goal: arithmetic, integer division, modulo with negatives, bitwise ops, `<cstdint>`. Exercise: implement `safe_add` that detects overflow. Gotchas: **signed overflow is UB**, unsigned wraparound, implicit promotions.
4. **Control flow** — Goal: `if`, `switch`, loops, range-for, structured bindings (preview). Exercise: FizzBuzz variant and prime sieve. Gotchas: `switch` fallthrough, loop index types (`size_t` vs `int`), dangling `else`.
5. **Functions** — Goal: parameters, return values, overloading, default args, pass by value vs reference, `const&`. Exercise: statistics functions (mean/median/mode). Gotchas: copies happen silently, default args are evaluated at call site, declaration order.
6. **Headers, source files, and the build model** — Goal: declarations vs definitions, include guards / `#pragma once`, the ODR, translation units, linking. Exercise: split a program into `.hpp`/`.cpp` (multi-file editor). Gotchas: ODR violations, `inline`, static init order (preview).
7. **I/O basics** — Goal: `std::cin`, `std::getline`, `std::string`, formatted output (`<format>` where available, else `<iomanip>`). Exercise: parse "name age" lines robustly. Gotchas: `cin >>` then `getline` newline trap, failed-extraction state.

*Capstone:* **Unit Converter CLI** — parse commands from stdin, convert between units, report errors gracefully. Tiers: Works / Robust.

### Module 2 — Data and Memory
*Test-out:* "Rotate and dedupe": rotate a `vector<int>` by k and remove duplicates preserving order.

1. **Arrays and `std::array`** — Goal: fixed-size sequences, bounds, decay. Exercise: matrix transpose on fixed arrays. Gotchas: **array-to-pointer decay**, no bounds checking, `sizeof` on parameters.
2. **`std::vector`** — Goal: dynamic arrays, `push_back`, `reserve`, capacity vs size, iterators. Exercise: implement run-length encoding. Gotchas: **iterator/reference invalidation**, `vector<bool>`, `[]` vs `at()`.
3. **`std::string` and `std::string_view`** — Goal: string ops, views, lifetimes. Exercise: word-frequency counter; tokenizer. Gotchas: **dangling `string_view`**, SSO, `std::string` is bytes not characters (UTF-8).
4. **References** — Goal: lvalue refs, `const&`, ref-returning functions. Exercise: swap/rotate helpers, "return the larger of two" returning a reference. Gotchas: returning a reference to a local (UB), reference members.
5. **Pointers and the stack/heap** — Goal: address-of, dereference, `nullptr`, pointer arithmetic, stack vs heap, `new`/`delete` *as a concept to understand and then avoid*. Exercise: hand-write a singly linked list with raw pointers (guided, with a leak-detecting test allocator). Gotchas: **dangling pointers, double free, leaks, use-after-free**, null deref.
6. **Structs and basic classes** — Goal: aggregates, member functions, access control, `this`. Exercise: `Point`, `Rectangle`, `Interval` with methods. Gotchas: struct padding, member init order = declaration order.
7. **Debug challenge: "Find the Bug" set** — 6 short programs with memory bugs (off-by-one, dangling reference, use-after-scope, iterator invalidation, uninitialized read, string_view dangling). Goal: diagnose using reasoning and (where supported) sanitizer output. Type: `debug`.

*Capstone:* **Inventory Tracker** — items in a `vector`, lookup, update, report; no raw `new`. Tiers: Works / Robust / Clean.

### Module 3 — Classes and Objects
*Test-out:* "Fraction class": a normalized `Fraction` with arithmetic and comparison operators.

1. **Constructors, initializer lists, `explicit`** — Exercise: `Money` class with validation. Gotchas: implicit conversions, `explicit`, delegating constructors, member init order warnings.
2. **Encapsulation and invariants** — Exercise: `BankAccount` that can never go negative. Gotchas: getters returning mutable references, `const` member functions.
3. **`const` correctness** — Exercise: fix a const-incorrect API. Gotchas: `mutable`, `const` propagation, `const` overloads.
4. **Operator overloading** — Exercise: `Vec2`/`Vec3` with `+ - * ==`, `<<`, `<=>` (spaceship). Gotchas: member vs non-member, symmetric operators, don't overload what is surprising.
5. **Static members, enums, `enum class`** — Exercise: state machine with `enum class`. Gotchas: unscoped enum leakage, static init order fiasco.
6. **Namespaces and modular design** — Exercise: reorganize a project into namespaces. Gotchas: `using namespace` pollution, ADL (argument-dependent lookup).

*Capstone:* **Library Management** — classes for books/members/loans with invariants and operators. Tiers: Works / Robust / Clean.

### Module 4 — Ownership and RAII (the heart of C++)
*Test-out:* "Leak-proof buffer": implement a resizable buffer class that never leaks or double-frees under copy and move.

1. **RAII** — Goal: resource lifetime = object lifetime. Exercise: a `FileHandle`/`ScopedLock`-style wrapper (simulated resource with a counting backend). Gotchas: exceptions and early returns, destructors must not throw.
2. **Rule of 0 / 3 / 5** — Exercise: implement a `String`-like class with copy ctor/assign/dtor, observed by a leak-checking allocator. Gotchas: shallow copy, self-assignment, copy-and-swap.
3. **Move semantics** — Goal: rvalue refs, `std::move`, move ctor/assign, `noexcept`. Exercise: add move operations to the `String` class; tests count copies vs moves. Gotchas: `std::move` doesn't move anything, moved-from state, returning locals (RVO/NRVO), `const` prevents moves.
4. **Smart pointers** — Goal: `unique_ptr`, `shared_ptr`, `weak_ptr`, `make_unique`/`make_shared`, custom deleters. Exercise: refactor the Module 2 linked list to `unique_ptr`; build an observer graph with `weak_ptr` to fix a cycle. Gotchas: `shared_ptr` cycles, `shared_ptr` overuse, passing smart pointers as parameters, `unique_ptr` for arrays.
5. **Lifetimes and value categories** — Goal: temporaries, lifetime extension, lvalue/rvalue/xvalue at a practical level. Exercise: predict-the-output puzzles about copies/moves/lifetimes. Gotchas: dangling via temporaries in range-for, reference to temporary.
6. **Exceptions and error handling** — Goal: `throw`/`catch`, exception safety guarantees (basic/strong/nothrow), `std::optional`, `std::expected` (C++23 if available, else a provided shim), error codes. Exercise: make a transactional operation strongly exception-safe. Gotchas: catching by value (slicing), destructor exceptions, cost model, when *not* to use exceptions.

*Capstone:* **Mini `unique_ptr` + `vector`** — implement simplified versions of both, passing ownership, move, and leak tests. Tiers: Works / Robust / Clean (no raw `new` outside the class internals).

### Module 5 — Generic Programming
*Test-out:* "Generic max-by": a templated function taking a range and a projection.

1. **Function templates** — Exercise: generic `clamp`, `min_of`, `swap`. Gotchas: template argument deduction, two-phase lookup, error messages.
2. **Class templates** — Exercise: `Stack<T>`, `Matrix<T, R, C>`. Gotchas: definitions in headers, dependent names (`typename`).
3. **Template specialization and `if constexpr`** — Exercise: pretty-printer that adapts by type. Gotchas: specialization vs overloading, instantiation bloat.
4. **Concepts (C++20)** — Exercise: constrain a numeric algorithm; write a custom concept. Gotchas: concept subsumption, readable constraint errors.
5. **Variadic templates and fold expressions** — Exercise: `sum(args...)`, a type-safe `printf`-like. Gotchas: parameter packs, perfect forwarding intro.
6. **`auto`, `decltype`, type traits** — Exercise: write `is_container<T>` via traits/concepts. Gotchas: `auto` strips references/const, `decltype(auto)`.

*Capstone:* **Generic Containers** — a templated ring buffer and fixed-capacity small-vector with iterators. Tiers: Works / Robust / Fast / Clean.

### Module 6 — The Standard Library
*Test-out:* "Top-K words": given text, return the k most frequent words using standard algorithms and containers.

1. **Containers tour** — `vector`, `deque`, `list`, `array`, `map`, `unordered_map`, `set`, `priority_queue`; when to use which and complexity. Exercise: pick the right container for 5 scenarios and prove with the Fast tier. Gotchas: `map::operator[]` inserts, unordered iteration order, hash quality.
2. **Iterators and ranges** — Goal: iterator categories, `<ranges>` views and pipelines. Exercise: rewrite loops as pipelines. Gotchas: iterator invalidation rules, view lifetimes.
3. **`<algorithm>`** — `sort`, `find_if`, `transform`, `accumulate`/`reduce`, `partition`, `remove`-erase idiom. Exercise: data-cleaning pipeline. Gotchas: `remove` doesn't erase, comparator strict weak ordering requirements.
4. **Lambdas and callables** — captures, `std::function` vs templates, `mutable`. Exercise: event/callback registry. Gotchas: dangling captures by reference, `std::function` overhead.
5. **Vocabulary types** — `optional`, `variant` + `visit`, `tuple`, `pair`, structured bindings, `span`. Exercise: tiny expression AST using `variant`. Gotchas: valueless-by-exception, `optional<T&>` isn't allowed.
6. **Strings, formatting, and text parsing** — `std::format`, `from_chars`/`to_chars`, `<charconv>`, `<regex>` (and why to be wary). Exercise: log-line parser.
7. **Time, randomness, and numerics** — `<chrono>`, `<random>`, `<numeric>`, floating-point pitfalls. Exercise: deterministic seeded simulation. Gotchas: `rand()` is bad, float equality, `std::chrono` units.

*Capstone:* **Log Analyzer** — parse a log, aggregate statistics, report top errors. Tiers: Works / Robust / Fast / Clean.

### Module 7 — Inheritance and Polymorphism
*Test-out:* "Shape area": a polymorphic shape hierarchy with correct virtual destruction.

1. **Inheritance basics** — Exercise: `Animal` → `Dog`/`Cat`. Gotchas: **object slicing**, access specifiers on bases.
2. **Virtual functions and dynamic dispatch** — Exercise: shape hierarchy with `override`/`final`. Gotchas: **non-virtual destructor in a base**, calling virtuals in ctors/dtors, `override` always.
3. **Abstract classes and interfaces** — Exercise: plugin-style logger interface with multiple backends. Gotchas: pure virtual with a body, interface segregation.
4. **Composition over inheritance** — Exercise: refactor a deep hierarchy into composition. Gotchas: fragile base class, diamond problem and virtual inheritance (awareness).
5. **Runtime polymorphism vs alternatives** — `std::variant` visitation, CRTP, type erasure (guided intro). Exercise: implement the same shapes three ways; compare via Fast tier. Gotchas: vtable cost, when static polymorphism wins.

*Capstone:* **Plugin Pipeline** — a text-processing pipeline with interchangeable stages using polymorphism and smart pointers. Tiers: Works / Robust / Clean.

### Module 8 — Concurrency *(may need server runner or WASM threads support; mark as "preview" in v1 if the WASM runner can't support it)*
*Test-out:* "Parallel sum": split a sum across threads correctly.

1. **Threads and `std::jthread`** — Exercise: spawn workers, join. Gotchas: forgetting to join, thread argument copying.
2. **Data races and mutexes** — Exercise: fix a racy counter with `mutex`/`lock_guard`/`scoped_lock`. Gotchas: **data race = UB**, lock ordering/deadlock, holding locks too long.
3. **Condition variables and queues** — Exercise: bounded blocking queue. Gotchas: spurious wakeups, lost notifications.
4. **Atomics and memory ordering (intro)** — Exercise: lock-free counter, spinlock. Gotchas: `memory_order_relaxed` pitfalls, ABA (awareness).
5. **Futures, async, and thread pools** — Exercise: build a simple thread pool. Gotchas: `std::async` launch policy, exceptions across threads.
6. **Coroutines (awareness)** — a generator example, with honest notes about complexity.

*Capstone:* **Job Scheduler** — thread pool with priorities and graceful shutdown.

> **Fallback if WASM can't run threads:** provide a **deterministic simulation** mode (cooperative scheduler harness) so concurrency *reasoning* can still be tested, and flag real-thread lessons as requiring the future server runner.

### Module 9 — Performance and Tooling
*Test-out:* "Make it fast": optimize a deliberately slow function to meet an ops budget.

1. **Measuring performance** — Goal: the "measure first" mindset, big-O vs constants, deterministic counters in this site. Exercise: identify the hot spot in a slow program.
2. **Memory layout and caches** — AoS vs SoA, locality, `std::vector` vs `std::list`. Exercise: restructure data for locality (scored via a cache-model counter or ops metric). Gotchas: false sharing, padding.
3. **Allocation costs and custom allocators** — arena/pool concepts, `std::pmr`. Exercise: write an arena allocator. Gotchas: alignment, lifetime of arena objects.
4. **Compile-time computation** — `constexpr`, `consteval`, `constinit`. Exercise: compile-time lookup table; `static_assert` tests.
5. **Build systems and project structure** — CMake concepts, headers vs sources, include-what-you-use mindset, warnings (explained conceptually since the site has no real build system).
6. **Tooling awareness** — sanitizers (ASan/UBSan/TSan), `clang-tidy`, `clang-format`, debuggers, Valgrind. Reading guide plus "reading sanitizer output" exercises with canned outputs.

*Capstone:* **Optimize the Pipeline** — given a correct-but-slow program, hit an ops/allocation budget without breaking tests. Tiers: Works / Fast / Clean.

### Module 10 — Hero Projects (integration)
Larger, multi-file projects combining everything, each with Works / Robust / Fast / Clean tiers. Choose any 3 to earn "Zero to Hero":
- **Mini Database** — in-memory key-value store with a query parser and transactions.
- **Expression Compiler** — tokenizer, parser, AST, evaluator, and simple optimizer.
- **Text Adventure Engine** — data-driven rooms/items/commands using `variant` and polymorphism.
- **Tiny Regex Engine** — NFA-based matcher.
- **JSON Library** — parser + serializer with good error messages.
- **Memory Allocator** — a malloc-style allocator with free lists, coalescing, and stats.

---

## 6. Domain Challenge Tracks

Domain tracks unlock progressively (each lists the core module needed). Challenges are written as **"You're an engineer at X and need to…"** with realistic requirements. Every challenge supports tiers: **Works → Robust → Fast → Clean**.

> Priority for first build: the owner has not yet picked favorite domains. Ship **Games, Finance, and Systems** first (they exercise the widest range of C++ strengths), then add the rest.

### 6.1 Games *(needs Modules 1–5)*
1. **Entity-Component System** — store entities/components cache-friendly; add/remove/iterate. (SoA, templates, Fast tier via cache/ops counters)
2. **Collision Detection** — AABB and circle tests, then a uniform-grid broadphase. (Math types, operators, algorithm complexity)
3. **A\* Pathfinding** — grid map, heuristic, priority queue; tests on tricky mazes. (Containers, `priority_queue`, performance)
4. **Object Pool Allocator** — fixed-size pool with O(1) alloc/free, no fragmentation. (Allocators, RAII, Fast)
5. **Game Loop and Fixed Timestep** — deterministic update loop with an injected clock. (`chrono`, determinism)
6. **Inventory & Item System** — `variant`-based item types with effects. (Polymorphism alternatives)

### 6.2 Finance / Trading *(needs Modules 1–6)*
1. **Fixed-Point Money Type** — no floating-point errors; correct rounding modes; overflow checks. (Operators, overflow, Robust)
2. **Order Book Matching Engine** — price-time priority matching, partial fills, cancels. (Containers, invariants, Fast)
3. **Moving Averages & Signals** — SMA/EMA over a stream with O(1) updates. (Ring buffer, numerics)
4. **Portfolio Risk Metrics** — returns, volatility, drawdown, VaR from a time series. (`<numeric>`, floating-point care)
5. **FIX-like Message Parser** — tag=value parser, zero-copy with `string_view`, validation. (Parsing, lifetimes)
6. **Low-Latency Event Queue** — SPSC ring buffer (threads track recommended; deterministic-sim fallback).

### 6.3 Systems / Infrastructure *(needs Modules 1–7)*
1. **LRU Cache** — O(1) get/put with list + hash map. (Containers, iterators)
2. **Arena / Bump Allocator** — alignment-correct, with reset. (Memory model)
3. **Thread-Safe Job Queue** — *(threads or sim-mode)*
4. **Memory-Pool with Leak Reporting** — track allocations, report leaks on shutdown. (RAII, instrumentation)
5. **Tiny Shell Tokenizer/Parser** — quotes, escapes, pipes (parsing only; no process spawning).
6. **Ring-Buffer Logger** — async-style logging with bounded memory.
7. **Consistent Hashing** — ring with virtual nodes and rebalancing counts.

### 6.4 Embedded / IoT *(needs Modules 1–5)*
1. **Fixed-Capacity Ring Buffer for Sensor Data** — no heap allocation allowed (Clean tier: no `new`, no `std::vector`).
2. **Bit-Field Register Manipulation** — set/clear/toggle fields of a simulated hardware register with `constexpr` helpers. (Bit ops)
3. **Thermostat State Machine** — `enum class` + transitions; table-driven; testable with fake sensors. (State machines)
4. **CRC / Checksum** — CRC-8/16 with a compile-time lookup table. (`constexpr`)
5. **Fixed-Point Math (Q-format)** — add/mul/div with saturation. (Overflow, operators)
6. **Packet Framing & Serialization** — byte-stuffed frames, endianness-safe encoding. (`<cstdint>`, `span`, UB avoidance)

### 6.5 Data & Scientific Computing *(needs Modules 1–6)*
1. **Matrix Class** — templated, operator overloads, multiplication; tier Fast via ops count. (Templates, loops, cache)
2. **CSV Parser** — quoted fields, escapes, ragged rows, good errors. (Parsing, robustness)
3. **Descriptive Statistics Library** — mean/variance (numerically stable: Welford), percentiles. (Numerics)
4. **N-Body Simulation** — deterministic integrator with energy-conservation tests. (Floating-point, structs)
5. **Linear Regression** — normal equations on small matrices. (Matrix, numerics)
6. **Sparse Vector/Matrix** — compare dense vs sparse representation costs. (Containers, complexity)

### 6.6 Networking / Parsing *(needs Modules 1–6)*
1. **HTTP Request Parser** — request line, headers, bodies; malformed input handling. (`string_view`, errors)
2. **URL Parser & Percent-Decoding** — RFC-ish rules with edge cases.
3. **JSON Tokenizer** — streaming tokens with accurate line/column errors.
4. **Binary Protocol Serializer** — varints, length-prefixed strings, endianness. (Bytes, UB avoidance)
5. **Base64 / Hex Codec** — table-driven, Fast tier.
6. **Rate Limiter (token bucket)** — injected clock, deterministic tests.

### 6.7 Graphics / Robotics *(needs Modules 1–5)*
1. **Vector & Quaternion Math** — operators, normalization, slerp.
2. **Ray–Sphere / Ray–Triangle Intersection** — correct handling of grazing and parallel cases. (Floating-point robustness)
3. **Forward Kinematics** — 2-link and N-link arms with transforms.
4. **PID Controller** — discrete implementation with anti-windup; tested against a simulated plant.
5. **Tiny Raytracer** — render to ASCII/PPM text and compare to golden output hash.
6. **Kalman Filter (1D)** — estimation with noise, tested via statistics.

### 6.8 Developer Tooling *(needs Modules 1–6)*
1. **Command-Line Argument Parser** — flags, options, subcommands, help text.
2. **Expression Evaluator** — shunting-yard or recursive descent with error positions.
3. **Tiny Regex Matcher** — literals, `.`, `*`, `+`, `?`, classes.
4. **Diff Tool** — LCS-based line diff with unified output.
5. **Template Engine** — `{{var}}` substitution with loops and escaping.
6. **Config File Parser (INI/TOML-lite)** — typed access with validation errors.

---

## 7. Badges

### 7.1 Principles
- Every badge is a **verifiable rule** evaluated against stored results (tests passed, tiers, hints, attempts), not self-reported.
- Reward good habits (independence, robustness, cleanliness), but **never penalize** asking for help.
- Visual system: shape/color by category; **bronze / silver / gold** for tiered badges.
- Badges are defined declaratively in `content/badges/*.yaml` so new ones can be added without code changes.

### 7.2 Badge definition schema

```yaml
- id: self-reliant
  name: "Self-Reliant"
  category: habits
  tier: bronze
  description: "Complete a challenge without using any hints or AI help."
  rule:
    type: lesson-count
    where:
      status: complete
      hintsUsed: 0
      aiHelpUsed: 0
      type: [challenge, capstone]
    atLeast: 1
```

Supported rule types (v1): `lesson-complete`, `lesson-count` (with filters), `module-complete`, `track-complete`, `tier-complete` (specific tier on N challenges), `first-try`, `streak-days`, `custom-event` (e.g. triggering a segfault, tracked via runner events).

### 7.3 Badge catalog

**Progress**
| Badge | Rule |
|---|---|
| Hello, World | Pass your first lesson |
| Module Master I–X | Complete each core module (by lessons or test-out) |
| Test-Out Artist | Pass 3 test-outs |
| Zero to Hero | Complete all core modules **and** 3 Hero Projects |

**Mastery tiers**
| Badge | Rule |
|---|---|
| It Works! (bronze/silver/gold) | Earn *Works* on 1 / 10 / 30 challenges |
| Bulletproof (bronze/silver/gold) | Earn *Robust* on 1 / 10 / 30 challenges |
| Need for Speed (bronze/silver/gold) | Earn *Fast* on 1 / 10 / 30 challenges |
| Squeaky Clean (bronze/silver/gold) | Earn *Clean* on 1 / 10 / 30 challenges |
| Flawless | All four tiers on a single challenge |
| Perfectionist | Flawless on 10 challenges |

**Domain**
| Badge | Rule |
|---|---|
| Game Dev, Quant, Embedded Engineer, Systems Hacker, Data Wrangler, Protocol Wizard, Graphics Guru, Toolsmith | Complete all challenges in the domain track (Works tier) |
| Domain Polyglot | Complete one challenge in every domain |

**Skills**
| Badge | Rule |
|---|---|
| Pointer Whisperer | Pass all Module 2 pointer lessons + the Find-the-Bug set |
| RAII Master | Pass Module 4 capstone at *Clean* tier |
| Move Semantics Ninja | Pass the move-semantics lesson with ≤ the optimal copy count |
| Template Wizard | Complete Module 5 capstone with Fast + Clean |
| Const Correct | Pass the const-correctness lesson at Clean |
| UB Slayer | Pass all "Find the Bug" debug challenges |
| Zero-Alloc | Complete an embedded challenge with allocation counter = 0 |
| Standard Bearer | Complete a challenge using only standard algorithms (no hand-written loops) |

**Learning habits**
| Badge | Rule |
|---|---|
| Self-Reliant (bronze/silver/gold) | Complete 1 / 10 / 25 challenges with zero hints and zero AI help |
| First Try | Pass a challenge on the very first run |
| Refactorer | After passing, submit a refactor that still passes with fewer lines or better tier |
| Good Question | Use the AI tutor in "explain" mode on a lesson and then pass it without reveal |
| Persistence | Pass a challenge after 15+ failed runs without revealing the solution |
| Fast Learner | Pass 3 test-outs in a single day |

**Consistency** *(keep low-pressure; streaks forgive one missed day per week)*
| Badge | Rule |
|---|---|
| On a Roll | 3-day streak |
| Habit Formed | 7-day streak |
| Dedicated | 30-day streak |

**Fun / hidden**
| Badge | Rule |
|---|---|
| Segfault Survivor | Trigger your first crash/trap (runner event) |
| Off By One | Fail a test with an off-by-one (detected via tagged test names) |
| It Compiles! | First clean compile after 10+ compiler errors in one session |
| Wall of Warnings | Fix 20 compiler warnings across lessons |
| Infinite Loop | Hit a timeout (we terminate the worker) |
| Night Owl / Early Bird | Complete a lesson between midnight–4am / 4–6am local time |

### 7.4 Badge UI
- Profile page: grid of badges (earned in color, unearned greyed with a hint of the requirement; hidden ones show "???").
- Toast notification on earning, with a link to the badge details and the **evidence** (e.g. which challenges counted).
- Optional: "Share badge" button that renders a PNG card client-side.

---

## 8. The AI Tutor (Bring Your Own Claude Key)

### 8.1 Setup and security
- User pastes an Anthropic API key in Settings; stored in `localStorage` only, never exported with progress and never sent anywhere except `api.anthropic.com`.
- The browser calls the Anthropic Messages API directly (requires the opt-in browser-access header, `anthropic-dangerous-direct-browser-access: true`; confirm current requirements in the official docs when implementing).
- Show a plain warning: *"Use a dedicated key with a spend limit. Anyone with access to this browser profile can read it."*
- Stream responses; cap `max_tokens`; let the user choose the model; show a rough token/cost indicator.
- **Future:** a server proxy option so no key is needed.

### 8.2 Context sent to Claude (and what is deliberately NOT sent)
**Send:**
- Lesson title, goals, and the lesson's explanatory text
- The learner's current code
- Latest compiler diagnostics and the **names and failure messages** of failing tests
- Number of attempts/hints used so far
- The selected help mode (below)

**Never send:**
- The reference solution
- The hidden test source code
- Other lessons' solutions

> This is the most important guard: if the model never sees the answer, it cannot leak it.

### 8.3 Help modes (a "hint ladder" the learner chooses)
1. **Nudge** — a guiding question or pointer to where to look. No code.
2. **Explain the concept** — explanation of the underlying idea, with a small example on a *different* problem.
3. **Explain my error** — plain-language explanation of what a compiler error or failing test means and why it happens, without writing the fix.
4. **Pinpoint** — identifies the line or area where the bug likely is, and why; no replacement code.
5. **Approach** — outlines the algorithm/approach in prose or pseudocode, not C++.
6. **Review (only after passing)** — code review of the working solution: idioms, style, alternatives, performance. Full code suggestions allowed here.

Each use increments `hintsUsed`/`aiHelpUsed` (shown transparently to the learner; badges like Self-Reliant read these).

### 8.4 System prompt (starting draft)

```
You are a patient C++ tutor inside a learning site called "CPP: From Zero to Hero".
The learner is an experienced programmer in other languages learning C++.

Your job is to help them LEARN, not to do the exercise for them.

Rules:
- Never write the solution or corrected code for the graded exercise. Do not provide
  code that would make the failing tests pass, even partially.
- You may show short examples of the SAME CONCEPT using a clearly DIFFERENT problem.
  Keep them under ~10 lines.
- Prefer asking one focused guiding question over a long lecture.
- When the learner is stuck on a compiler error, explain what it means and why it
  happens; point at the line; let them make the fix.
- Mention C++-specific pitfalls (undefined behavior, ownership, lifetimes) when relevant,
  and relate to what they might know from other languages.
- Respect the help mode requested by the app: {{mode}}. If the learner asks for more help
  than that mode allows, suggest they escalate to the next mode in the app.
- If the learner explicitly demands the answer, kindly decline and offer the next-level
  hint instead. The app has a separate "Reveal solution" button for that.
- Be concise, warm, and encouraging. No filler.
- Only after the learner's solution passes all tests (mode = review) may you suggest
  concrete code improvements.

Context:
Lesson: {{title}} — {{goals}}
Failing tests: {{failingTests}}
Compiler diagnostics: {{diagnostics}}
Learner's code:
{{code}}
```

### 8.5 Output guardrail (defense in depth)
Before showing a reply in modes 1–5:
1. Extract code blocks from the response.
2. If a block exceeds ~12 lines **or** compiles together with the hidden tests and passes → treat as a leak: discard and re-ask the model once with "Respond again without writing solution code"; if it still fails, show a safe fallback message and the static hint for the current level.
3. Log (locally) leak catches so the prompt can be tuned.

> Honest limitation: a determined learner can always get answers elsewhere or prompt-inject around this. The goal is to nudge good learning habits, not enforce security. Withholding the solution from the model's context is what makes it robust.

### 8.6 Additional tutor features
- "Explain this compiler error" button on any diagnostic.
- "Quiz me" — generate 3 quick concept-check questions for the finished module.
- "Another one like this" — generate a fresh practice problem on a weak concept (not auto-graded unless a test is also generated; optional/experimental).
- Conversation memory scoped to the current lesson only; cleared on lesson change.

---

## 9. UI / UX

### 9.1 Main views
1. **Home / Track map** — visual path of modules with progress rings; domain tracks branch off; pace toggle (Full / Fast) and "Test out" buttons.
2. **Lesson view** — three-pane layout:
   - Left: lesson text (Markdown), tabs for *Lesson / Fast summary / Gotchas / Hints*.
   - Center: code editor (multi-file tabs), **Run** and **Run Tests** buttons, reset-to-starter.
   - Right/bottom: output console, **Test results checklist** (grouped by tier), compiler diagnostics (with friendly text), AI tutor panel.
3. **Profile** — badges, streaks, per-module progress, export/import progress.
4. **Settings** — theme, editor options (font size, vim/emacs keybindings optional), pace default, API key, data export.

### 9.2 Behavior details
- First load: toolchain download progress with explanation; subsequent loads instant from cache.
- **Run** compiles and executes `main` with optional stdin box; **Run Tests** compiles with the hidden tests.
- Compile errors appear as editor squiggles + a list; clicking jumps to line.
- Test results: green/red per test, tier headings; failure messages shown; tier progress bar (Works ▸ Robust ▸ Fast ▸ Clean).
- Hint ladder shown as locked steps; revealing one is explicit and counted.
- "Reveal solution" is gated behind a confirm dialog and records `solutionRevealed` (the lesson is still completable, but certain badges require no reveal).
- Accessibility: keyboard navigation, screen-reader labels, color-blind-safe pass/fail indicators (icons, not just color), adjustable font size.
- Mobile: functional but desktop-first (code editing on phones is rough); lesson text readable on mobile.

---

## 10. Roadmap

**M0 — Feasibility spike (do first; biggest risk)**
- Get a WASM Clang running in a Web Worker compiling and running a C++20 program with `<iostream>`, `<vector>`, `<string>`, `<algorithm>`, `<memory>`.
- Measure: first-load size, cached load time, compile time for a ~50-line program, memory use.
- Prove worker termination handles infinite loops.
- **Decision gate:** if compile time is unacceptable (> ~8–10 s typical) or key headers are missing, switch the Runner implementation to a server backend (Piston/Judge0 or custom container) — the interface in 2.2 is the same.

**M1 — Walking skeleton**
- Vite + TS app shell, Monaco editor, Runner interface with the WASM implementation, console output, diagnostics as squiggles.
- One hard-coded lesson with the `zth_test.hpp` harness and a test checklist UI.

**M2 — Content pipeline**
- Content directory loader, `lesson.yaml` / `hints.yaml`, Markdown rendering, multi-file editor.
- CI validation script (Section 3.4).
- Module 1 fully authored (7 lessons + capstone + test-out).

**M3 — Progress, tiers, badges v1**
- IndexedDB progress store, export/import, tiers in the test UI, badge engine (declarative YAML + rule evaluator) with Progress and Mastery badges.
- Pace toggle (Full / Fast) and test-out flow.

**M4 — Friendly diagnostics + hint ladder**
- Friendly error table (first ~30 rules), static hint ladder UI, reveal-solution flow.

**M5 — AI tutor**
- Settings for API key, help modes, system prompt, streaming UI, context builder (no solutions!), output guardrail with leak detection.

**M6 — Content: Modules 2–4** (memory, classes, RAII) including the Find-the-Bug debug set and instrumented allocator/test types for leak and copy/move counting.

**M7 — Domain tracks v1** — Games, Finance, Systems (6 challenges each).

**M8 — Modules 5–7**, then Module 9 and 10; then remaining domain tracks.

**M9 — Concurrency (Module 8)** — decide between WASM threads (requires cross-origin isolation headers: COOP/COEP; check hosting support) vs deterministic simulation vs the server runner.

**M10 — Polish** — accessibility pass, theming, performance, shareable badge cards, analytics-free by default.

**Later / optional:** accounts + sync (backend + DB), server runner for full-fidelity C++ and sanitizers, server proxy for the AI tutor, community-authored challenges, C++23 track, leaderboards.

---

## 11. Risks & Open Questions

| Risk / Question | Notes / Mitigation |
|---|---|
| WASM Clang size and compile speed | Spike in M0; cache aggressively; precompiled headers if possible; fallback to server runner behind the same interface |
| Library coverage in the WASM toolchain (`<format>`, `<ranges>`, `<regex>`, threads, `std::expected`) | Document supported subset per lesson; provide shims (e.g. a minimal `expected`) where needed |
| Deterministic "Fast" tier | Use op/allocation counters and scaling tests, never raw wall-clock |
| Sanitizers in WASM | Likely unavailable; teach via canned sanitizer output and instrumented allocators/types |
| Content volume | The curriculum above is large. Ship Module 1 + one domain first, validate the loop, then expand. Use the CI validator to keep quality |
| AI tutor leaking answers | Withhold solutions/tests from context; output guardrail; hint-ladder UI; accept that it's a nudge, not a lock |
| API key exposure in browser | Clear warnings, dedicated-key advice, future proxy option |
| Hosting needs for threads (COOP/COEP) | Pick a host that allows custom headers (Cloudflare Pages/Netlify) if WASM threads are desired |
| Licensing of the compiler toolchain and any bundled libraries | Verify licenses (LLVM is Apache-2.0 with LLVM exceptions) and include attribution |

**Open questions for the owner**
1. Which domains matter most? (Defaults: Games, Finance, Systems first.)
2. Is Module 8 (concurrency) important enough to justify the server runner early?
3. Preferred frontend framework (React vs Svelte)?
4. Should the site be public eventually (affects whether to plan the backend/accounts earlier)?
5. Any specific C++ standard preference (C++20 now, C++23 later)?
