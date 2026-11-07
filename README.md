# CPP: From Zero to Hero

An interactive C++ course: write code in the browser, run it against real unit tests, get immediate feedback.
Full product spec: [project_assets/CPP_From_Zero_to_Hero_SPEC.md](project_assets/CPP_From_Zero_to_Hero_SPEC.md).

## Status

| Area | State |
|---|---|
| Runner (Docker sandbox + API) | Working, with attack tests |
| Front end (lesson page, editor, tiers, hints, progress) | Working, local storage progress |
| Module 1 Foundations | **Complete**: test-out, 7 lessons, capstone (9 items, 51 mutant solutions that must fail) |
| WASM runner for the static build | Spiked ([spike/](spike/)); not wired into the app yet |
| Friendly compiler errors, badges, AI tutor, Modules 2-10, domain tracks | Planned (see the spec) |

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
cd tools/validate-content && npm install && node validate.mjs   # every lesson: starter fails, solution passes, 0 warnings
cd tools/e2e && npm install && npx playwright install chromium && node run.mjs   # drives the real UI in a browser
```

The e2e script needs the runner (`docker compose up -d runner`) and the dev server running. Set `SHOTS=<dir>` to save screenshots.

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
