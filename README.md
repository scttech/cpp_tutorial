# CPP: From Zero to Hero

An interactive C++ course: write code in the browser, run it against real unit tests, get immediate feedback.
Full product spec: [project_assets/CPP_From_Zero_to_Hero_SPEC.md](project_assets/CPP_From_Zero_to_Hero_SPEC.md).

**[Try the live demo](https://scttech.github.io/cpp_tutorial/)**: the static build, with the compiler running in your browser (no install). Some lessons need the full Docker build and are greyed out there. See [Two ways to run it](#two-ways-to-run-it).

## Status

| Area | State |
|---|---|
| Runner (Docker sandbox + API) | Working, with attack tests |
| Front end (lesson page, editor, tiers, hints) | Working |
| Progress persistence | IndexedDB (localStorage fallback), export/import JSON, migration from the first release, streaks |
| Badges | 44 declarative badges (progress, mastery, habits, consistency, hidden), Profile page, toasts |
| Friendly compiler errors | 50 rules, each verified against real Clang output; raw message always one click away |
| Module 1 Foundations | **Complete**: test-out, 7 lessons, capstone (9 items, 51 mutant solutions that must fail) |
| Module 2 Data and Memory | **Complete**: test-out, 7 lessons (arrays, vector, strings, references, pointers and the heap, structs, a Find-the-Bug debug set) and the Inventory Tracker capstone: 9 items, 130 mutant solutions in total that must fail. Uses all four tiers: Fast = counted heap allocations, Clean = leak/double-free detection and no raw `new` |
| Module 3 Classes and Objects | **Complete**: test-out (a normalized `Fraction` with operators), six lessons (constructors/`explicit`, encapsulation and invariants, const correctness with `mutable` caches, operator overloading, `enum class` state machines with `static` members, namespaces/ADL/header hygiene) and the Library Management capstone: 8 items, 244 mutant solutions in total that must fail. Uses the Works, Robust and Clean tiers (Clean scans the header for `using namespace`, code outside the namespace, and raw `new`/`delete`) |
| Module 4 Ownership and RAII | **Complete**: test-out (a `Buffer` that follows the rule of five), five lessons (destructors and RAII, copying and the rule of three, moving and the rule of five, `unique_ptr` and the rule of zero, `shared_ptr` and `weak_ptr`), a debug challenge (six lifetime bugs, each a sanitizer report) and the `IntVector` capstone: 8 items, 219 mutant solutions in total that must fail. Every lesson runs under AddressSanitizer and UBSan with allocation tracking; the Fast tier counts allocations (a move, a swap and a sort must allocate nothing) |
| Sanitizers (Docker runner) | Working: a lesson turns them on with `flags: [..., "-fsanitize=address,undefined"]` and `requires: [sanitizers]`. Reports are symbolized, parsed (kind, file:line, where the memory was freed and allocated) and explained by [content/diagnostics/runtime.yaml](content/diagnostics/runtime.yaml): 18 rules, each verified against real sanitizer output. Module 4 uses them in every lesson |
| Test harness instrumentation | Allocation tracking (leaks, double free, `new[]`/`delete` mismatch, poisoned freed memory) and a copy/move-counting `Tracked` type |
| Static build (in-browser compiler) | Working: [wasm-runner/](wasm-runner/) runs Clang/LLD as WebAssembly in Workers. 16 of 34 lessons run in it ([content/wasm-compat.json](content/wasm-compat.json)); the rest are greyed out. The GitHub Pages workflow ([.github/workflows/pages.yml](.github/workflows/pages.yml)) builds and deploys it |
| AI tutor, Modules 5-10, domain tracks | Planned (see the spec) |

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
every push to `main`, so it tracks the docker-compose version. Lessons the browser compiler cannot teach are greyed out and marked
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
| `runner-image/` | The sandbox image (Clang, GCC, and the in-container job runner) |
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
cd tools/e2e && npm install && npx playwright install chromium && npm test   # drives the real UI in a browser (6 scripts; wasm.mjs separately, see above)
cd wasm-runner && npm install && npm test            # the in-browser (WASM) runner on small programs; no Docker needed
cd tools/validate-content && npm run compat          # which lessons run in the browser? (incremental; writes content/wasm-compat.json)
```

The validator and e2e need the runner (`docker compose up -d runner`); e2e also needs the dev server. Set `SHOTS=<dir>` to save screenshots.

## Authoring content

- **A badge**: add an entry to a file in [content/badges/](content/badges/). Rules are data (`lesson-count`, `module-complete`, `tier-complete`, `streak-days`, ...), so no code change is needed. Badges whose rules refer to content that does not exist yet appear under "coming soon".
- **A friendly error**: add a rule to [content/diagnostics/friendly.yaml](content/diagnostics/friendly.yaml) with a `trigger` program. `npm run validate` compiles every trigger with real Clang and fails if the rule does not fire, or if an earlier rule shadows it.
- **A lesson**: see [content/tracks/](content/tracks/); every lesson needs a starter, solution, hidden tests, hints and ideally a `mutants/` folder of plausible wrong solutions that the tests must reject.
  - Memory lessons: put `#define ZTH_TRACK_ALLOCATIONS` at the top of `tests/test.cpp`, then use `zth::mem::Scope` with `ZTH_EXPECT_NO_MEMORY_BUGS(scope)` / `ZTH_EXPECT_ALLOCS_AT_MOST(scope, n)`, and `zth::Tracked` to count copies and moves. See [runner-image/include/zth_test.hpp](runner-image/include/zth_test.hpp).
  - A module whose lessons are not all written yet sets `inProgress: true` in its `module.yaml`: finishing the lessons that exist does not complete it or award its Module Master badge (the test-out still can). Remove the flag when the module is finished (Modules 1-4 are).
  - Sanitizer lessons: put `-fsanitize=address,undefined` in the lesson's `flags` and `requires: [sanitizers]` (the static build then greys the lesson out); raise `limits.memoryMb` to 256 (sanitized programs use more memory). A sanitizer report ends only the test it happened in (status "Crashed", `stopped by a sanitizer: ...`); the results panel explains the first report in plain language and offers the raw one. Leaks are reported by LeakSanitizer in plain runs; in tests use `ZTH_TRACK_ALLOCATIONS`, which also poisons freed blocks under AddressSanitizer so a read after `delete` is a report, not a silent garbage value. To add or change an explanation, edit [content/diagnostics/runtime.yaml](content/diagnostics/runtime.yaml) (every rule needs a `trigger` program, checked by `runtime-check.mjs`).
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
