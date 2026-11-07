# CPP: From Zero to Hero

An interactive C++ course: write code in the browser, run it against real unit tests, get immediate feedback.
Full product spec: [project_assets/CPP_From_Zero_to_Hero_SPEC.md](project_assets/CPP_From_Zero_to_Hero_SPEC.md).

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
| Test harness instrumentation | Allocation tracking (leaks, double free, `new[]`/`delete` mismatch, poisoned freed memory) and a copy/move-counting `Tracked` type |
| WASM runner for the static build | Spiked ([spike/](spike/)); not wired into the app yet |
| AI tutor, Modules 3-10, domain tracks | Planned (see the spec) |

## Two ways to run it

| | **Static build** (GitHub Pages) | **Full build** (docker-compose) |
|---|---|---|
| Compiler | Clang compiled to WebAssembly, in your browser | Real Clang 18 / GCC 14 in a sandbox container |
| Exceptions (`try`/`catch`) | No | Yes |
| Threads / concurrency | No | Yes |
| Sanitizers (ASan/UBSan/TSan) | No | Planned |
| First load | ~90 MB toolchain download (cached) | Nothing to download |
| Lessons needing the server | Shown greyed out, marked "server needed" | Available |

The static build is a self-contained demo and is frozen at its last release; ongoing development continues in the
docker-compose version. Both share the same UI, lesson format and `Runner` interface.

## Layout

| Path | What |
|---|---|
| `app/` | Vite + React + TypeScript front end |
| `runner-service/` | Node/Fastify API: validates requests, runs each job in a fresh sandbox container |
| `runner-image/` | The sandbox image (Clang, GCC, and the in-container job runner) |
| `spike/` | M0 feasibility spike for the WASM (browsercc) runner |
| `docker-compose.yml` | Runner service + Docker socket proxy |

## Run the backend

```bash
docker compose --profile build-only build   # builds zth-sandbox and the runner
docker compose up -d runner                 # API on http://localhost:8080
curl localhost:8080/healthz
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
cd app && npm test                                   # 99 unit tests: badge engine, streaks, run recording, import validation, friendly-rule matching
cd tools/validate-content && npm install && npm run validate   # the harness itself (allocation tracking), every lesson (starter fails, solution passes, mutants fail), every friendly rule vs real Clang
cd tools/e2e && npm install && npx playwright install chromium && npm test   # drives the real UI in a browser (4 scripts)
```

The validator and e2e need the runner (`docker compose up -d runner`); e2e also needs the dev server. Set `SHOTS=<dir>` to save screenshots.

## Authoring content

- **A badge**: add an entry to a file in [content/badges/](content/badges/). Rules are data (`lesson-count`, `module-complete`, `tier-complete`, `streak-days`, ...), so no code change is needed. Badges whose rules refer to content that does not exist yet appear under "coming soon".
- **A friendly error**: add a rule to [content/diagnostics/friendly.yaml](content/diagnostics/friendly.yaml) with a `trigger` program. `npm run validate` compiles every trigger with real Clang and fails if the rule does not fire, or if an earlier rule shadows it.
- **A lesson**: see [content/tracks/](content/tracks/); every lesson needs a starter, solution, hidden tests, hints and ideally a `mutants/` folder of plausible wrong solutions that the tests must reject.
  - Memory lessons: put `#define ZTH_TRACK_ALLOCATIONS` at the top of `tests/test.cpp`, then use `zth::mem::Scope` with `ZTH_EXPECT_NO_MEMORY_BUGS(scope)` / `ZTH_EXPECT_ALLOCS_AT_MOST(scope, n)`, and `zth::Tracked` to count copies and moves. See [runner-image/include/zth_test.hpp](runner-image/include/zth_test.hpp).
  - A module whose lessons are not all written yet sets `inProgress: true` in its `module.yaml`: finishing the lessons that exist does not complete it or award its Module Master badge (the test-out still can). Remove the flag when the module is finished.
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
