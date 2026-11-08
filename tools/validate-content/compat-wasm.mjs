// WASM compatibility check: can each lesson be taught by the in-browser runner (the static GitHub Pages build)?
//
// A lesson is compatible when the validator's own invariants hold under the WASM runner:
//   - the reference solution builds with ZERO diagnostics and passes every test
//   - the starter still fails (unless it is meant not to build)
//   - every mutant (plausible wrong solution) is still rejected
// Anything else (exceptions, threads, a crash that WebAssembly does not reproduce, a different warning...) makes the
// lesson "server needed": the app greys it out in the static build. The result is committed as
// content/wasm-compat.json. It is incremental: a lesson is only re-run when its files, the harness or the runner
// changed (its hash is stored in the report).
//
//   node compat-wasm.mjs [lesson-id-substring]   check (changed lessons only) and write the report
//   --force      re-run every lesson          --check   run nothing; exit 1 if the report is missing or stale
//   --jobs N     parallel compilers (default: half the CPUs)      --verbose   print every problem found
import { createHash } from "node:crypto";
import { readFile, readdir, writeFile } from "node:fs/promises";
import { join } from "node:path";
import { fileURLToPath } from "node:url";
import { CONTENT_ROOT, findLessonDirs, loadLesson, walk } from "./lessons.mjs";
import { createNodeRunner } from "../../wasm-runner/src/node-host.mjs";

const args = process.argv.slice(2);
const flag = (n) => args.includes(n);
const opt = (n) => (args.includes(n) ? args[args.indexOf(n) + 1] : undefined);
const filter = args.find((a, i) => !a.startsWith("--") && args[i - 1] !== "--jobs");
const REPORT = join(CONTENT_ROOT, "wasm-compat.json");
const RUNNER_DIR = fileURLToPath(new URL("../../wasm-runner/", import.meta.url));
const HEADER_DIR = fileURLToPath(new URL("../../runner-image/include/", import.meta.url));

// ---- what a lesson's result depends on ----
async function inputsHash(dir) {
  const h = createHash("sha256");
  // Line endings are normalized: a Windows checkout (CRLF) and CI (LF) must agree on the hash.
  const add = async (p, label) => h.update(label + "\0").update((await readFile(p, "latin1")).replace(/\r\n/g, "\n"), "latin1").update("\0");
  for await (const p of walk(dir)) await add(p, "lesson:" + p.slice(dir.length));
  for (const f of (await readdir(HEADER_DIR)).sort()) await add(join(HEADER_DIR, f), "header:" + f);
  for (const f of (await readdir(join(RUNNER_DIR, "src"))).sort()) await add(join(RUNNER_DIR, "src", f), "runner:" + f);
  h.update(JSON.parse(await readFile(join(RUNNER_DIR, "node_modules/browsercc/package.json"), "utf8")).version);
  return h.digest("hex").slice(0, 16);
}

// ---- judging one lesson ----
const first = (s, n = 280) => s.trim().split("\n").slice(0, 4).join(" | ").slice(0, n);

function whyNot(problems) {
  const text = problems.join("\n");
  if (/exceptions disabled|__cxa_|std::thread|<thread>|pthread|<mutex>|<future>|<atomic>.*thread/i.test(text)) {
    if (/exception|__cxa_/i.test(text)) return "needs C++ exceptions";
    return "needs threads";
  }
  if (/sanitize/i.test(text)) return "needs sanitizers";
  if (/does not build/.test(problems[0])) return "uses a C++ library feature the browser build lacks";
  // The browser build is wasm32: size_t, pointers and long are 4 bytes, so arithmetic that relies on 64 bits differs.
  return "gives different results in the browser (it is a 32-bit build)";
}

async function check(runner, dir) {
  const L = await loadLesson(dir);
  const { lesson } = L;
  const need = (lesson.requires ?? []).filter(Boolean);
  if (need.length) return { wasm: false, reason: `needs ${need.join(", ")}`, declared: true };

  const run = (userFiles) => runner.run({
    mode: "test", userFiles, testFiles: L.tests,
    flags: lesson.flags ?? ["-std=c++20", "-Wall", "-Wextra"], limits: lesson.limits ?? {},
  });
  const problems = [];

  const sol = await run({ ...L.starterReadonly, ...L.solution });
  if (!sol.compiled) problems.push(`solution does not build: ${first(sol.compileOutput)}`);
  else {
    if (sol.compileOutput.trim()) problems.push(`solution builds with diagnostics: ${first(sol.compileOutput)}`);
    for (const t of sol.tests) if (t.status !== "pass") problems.push(`solution fails "${t.name}" [${t.status}]: ${first(t.message ?? "", 200)}`);
    if (!sol.tests.length) problems.push("solution produced no test results");
  }
  // No point judging the starter and mutants if the solution itself does not work here.
  if (problems.length) return { wasm: false, reason: whyNot(problems), details: problems.slice(0, 5) };

  const jobs = [];
  jobs.push(run(L.starter).then((r) => {
    if (!r.compiled && !lesson.starterBroken) problems.push(`starter does not build: ${first(r.compileOutput)}`);
    if (r.compiled && r.tests.length && r.tests.every((t) => t.status === "pass")) problems.push("starter passes every test");
    if (r.compiled && !r.tests.length) problems.push("starter produced no test results");
  }));
  for (const [name, files0] of Object.entries(L.mutants)) {
    const { EXPECT_COMPILE_FAIL, ...files } = files0;
    const expectBuildFail = EXPECT_COMPILE_FAIL !== undefined;
    jobs.push(run({ ...L.starterReadonly, ...files }).then((r) => {
      if (expectBuildFail) { if (r.compiled) problems.push(`mutant "${name}" should not build but does`); }
      else if (!r.compiled) problems.push(`mutant "${name}" does not build: ${first(r.compileOutput)}`);
      else if (r.tests.every((t) => t.status === "pass")) problems.push(`mutant "${name}" passes every test (it is only caught on the server)`);
    }));
  }
  await Promise.all(jobs);
  if (problems.length) return { wasm: false, reason: whyNot(problems), details: problems.sort().slice(0, 8) };
  return { wasm: true, mutants: Object.keys(L.mutants).length };
}

// ---- main ----
const dirs = await findLessonDirs();
const lessons = [];
for (const dir of dirs) {
  const id = (await loadLesson(dir)).lesson.id;
  if (filter && !id.includes(filter)) continue;
  lessons.push({ dir, id, hash: await inputsHash(dir) });
}

let report = { toolchain: "browsercc", lessons: {} };
try { report = JSON.parse(await readFile(REPORT, "utf8")); } catch { /* first run */ }
const stale = lessons.filter((l) => flag("--force") || report.lessons[l.id]?.hash !== l.hash);

if (flag("--check")) {
  for (const l of stale) console.log(`stale  ${l.id}`);
  const known = new Set(dirs.length ? (await Promise.all(dirs.map(async (d) => (await loadLesson(d)).lesson.id))) : []);
  const orphans = Object.keys(report.lessons).filter((id) => !known.has(id) && !filter);
  for (const id of orphans) console.log(`orphan ${id} (no such lesson any more)`);
  console.log(stale.length || orphans.length ? "\nwasm-compat.json is out of date: run `npm run compat`" : "wasm-compat.json is up to date");
  process.exit(stale.length || orphans.length ? 1 : 0);
}

const runner = createNodeRunner({ workers: Number(opt("--jobs")) || undefined });
const t0 = Date.now();
let done = 0;
try {
  // Lessons are independent; the runner's worker pool bounds the real parallelism.
  const queue = [...stale];
  const lanes = Math.max(2, Number(opt("--jobs")) || 4);
  await Promise.all(Array.from({ length: lanes }, async () => {
    for (let l; (l = queue.shift()); ) {
      const t = Date.now();
      let r;
      try { r = await check(runner, l.dir); } catch (e) { r = { wasm: false, reason: "the check itself failed", details: [String(e?.stack ?? e).slice(0, 400)] }; }
      report.lessons[l.id] = { ...r, hash: l.hash };
      const secs = ((Date.now() - t) / 1000).toFixed(0);
      console.log(`${r.wasm ? "ok     " : "SERVER "} ${l.id}  (${++done}/${stale.length}, ${secs}s)${r.wasm ? `  ${r.mutants} mutants rejected` : `  ${r.reason}`}`);
      if (!r.wasm) for (const d of (r.details ?? []).slice(0, flag("--verbose") ? 99 : 2)) console.log(`          - ${d}`);
    }
  }));
} finally {
  await runner.close();
}

if (!filter) for (const id of Object.keys(report.lessons)) if (!lessons.some((l) => l.id === id)) delete report.lessons[id];
report.lessons = Object.fromEntries(Object.entries(report.lessons).sort(([a], [b]) => a.localeCompare(b)));
await writeFile(REPORT, JSON.stringify(report, null, 2) + "\n");

const all = Object.values(report.lessons);
console.log(`\n${stale.length ? `checked ${stale.length} lesson(s) in ${((Date.now() - t0) / 1000).toFixed(0)}s; ` : "nothing to do; "}${all.filter((l) => l.wasm).length}/${all.length} lessons run in the browser`);
