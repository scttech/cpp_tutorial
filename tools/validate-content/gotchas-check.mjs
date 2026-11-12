// Checks the "C++ gotchas" explanations (spec 3.2 / 9.1): each lesson lists gotcha ids in lesson.yaml and explains them in
// a gotchas.yaml next to it:
//
//   array-decay:
//     title: "An array decays to a pointer"
//     text: "One or two sentences: what goes wrong and why."
//
// Errors (always fail): a malformed file, an entry the lesson does not list, a missing/odd title or text, a title or
// text outside its length limits, a duplicate id in lesson.yaml.
// Missing explanations are reported as coverage; they fail only with --strict (use it once all lessons are written).
//
// An entry may also have a `demo`: two tiny programs (`bad` and `fix`, each a complete main.cpp) that the Gotchas tab lets the
// learner run. Their structure is always checked. With --run every program is compiled and run on the Docker runner, with --wasm
// (demos without sanitize or server only) on the in-browser compiler, and each result is compared with what the demo declares:
//   out      the exact stdout              error    a compile error containing this text      warns    a warning containing this text
//   reports  the sanitizer report (stderr) contains this text (the demo then has `sanitize: true`)
// A `fix` must build with no diagnostics at all and print exactly `out` (and nothing on stderr under a sanitizer).
//
//   node gotchas-check.mjs [lesson-substring] [--strict] [--list] [--run] [--wasm]
//     --list names every lesson that is not fully covered;  --run needs the runner (RUNNER_URL, default http://localhost:8080)
import { readFile } from "node:fs/promises";
import { join, relative, sep } from "node:path";
import { parse } from "yaml";
import { CONTENT_ROOT, findLessonDirs } from "./lessons.mjs";

const args = process.argv.slice(2);
const strict = args.includes("--strict");
const list = args.includes("--list");
const runDocker = args.includes("--run");
const runWasm = args.includes("--wasm");
const filter = args.find((a) => !a.startsWith("--"));

export const LIMITS = { title: [3, 70], text: [40, 330] };
const ID = /^[a-z0-9]+(-[a-z0-9]+)*$/;

const problems = [];
const demos = []; // every structurally valid demo, for --run and --wasm

const DEMO_LIMITS = { lines: 20, chars: 1200 };

function checkDemo(id, d, bad, rel) {
  const at = (m) => bad(`"${id}": demo ${m}`);
  if (typeof d !== "object" || d === null || Array.isArray(d)) return at("must be a mapping with bad and fix");
  const extra = Object.keys(d).filter((k) => !["bad", "fix", "sanitize", "server"].includes(k));
  if (extra.length) at(`has an unknown field ${extra.join(", ")}`);
  for (const k of ["sanitize", "server"]) if (d[k] !== undefined && typeof d[k] !== "boolean") at(`${k} must be true or false`);
  let ok = true;
  for (const kind of ["bad", "fix"]) {
    const sn = d[kind];
    if (typeof sn !== "object" || sn === null) {
      at(`needs a ${kind} program`);
      ok = false;
      continue;
    }
    const unknown = Object.keys(sn).filter((k) => !["code", "out", "error", "reports", "warns"].includes(k));
    if (unknown.length) at(`${kind} has an unknown field ${unknown.join(", ")}`);
    if (typeof sn.code !== "string" || sn.code.trim() === "") {
      at(`${kind} needs code`);
      ok = false;
      continue;
    }
    const lines = sn.code.replace(/\n+$/, "").split("\n").length;
    if (lines > DEMO_LIMITS.lines || sn.code.length > DEMO_LIMITS.chars) at(`${kind} is too long (${lines} lines, ${sn.code.length} characters; limits ${DEMO_LIMITS.lines} and ${DEMO_LIMITS.chars}): a demo is a few lines`);
    if (!/\bmain\s*\(/.test(sn.code)) at(`${kind} has no main()`);
    for (const k of ["out", "error", "reports", "warns"]) if (sn[k] !== undefined && typeof sn[k] !== "string") at(`${kind}.${k} must be a string`);
    if (kind === "bad" && !["out", "error", "reports", "warns"].some((k) => sn[k] !== undefined)) at("bad must say what it shows: out, error, reports or warns");
    if (kind === "fix") {
      if (typeof sn.out !== "string") at("fix needs out (its exact stdout)");
      for (const k of ["error", "reports", "warns"]) if (sn[k] !== undefined) at(`fix must not have ${k}: it has to run clean`);
    }
    if (sn.reports !== undefined && d.sanitize !== true) at("reports needs sanitize: true");
  }
  if (ok) demos.push({ rel, id, demo: d });
}
let ids = 0;
let covered = 0;
const partial = [];

for (const dir of await findLessonDirs()) {
  const rel = relative(CONTENT_ROOT, dir).split(sep).join("/");
  if (filter && !rel.includes(filter)) continue;
  const lesson = parse(await readFile(join(dir, "lesson.yaml"), "utf8"));
  const listed = lesson.gotchas ?? [];
  const bad = (msg) => problems.push(`${rel}: ${msg}`);

  if (!Array.isArray(listed)) {
    bad("gotchas in lesson.yaml must be a list of ids");
    continue;
  }
  for (const id of listed) if (typeof id !== "string" || !ID.test(id)) bad(`gotcha id "${String(id)}" must be kebab-case`);
  if (new Set(listed).size !== listed.length) bad("a gotcha id is listed twice in lesson.yaml");
  ids += listed.length;

  let doc = {};
  let raw;
  try {
    raw = await readFile(join(dir, "gotchas.yaml"), "utf8");
  } catch {
    raw = undefined;
  }
  if (raw !== undefined) {
    try {
      const v = parse(raw);
      if (typeof v !== "object" || v === null || Array.isArray(v)) bad("gotchas.yaml must be a mapping of id to {title, text}");
      else doc = v;
    } catch (e) {
      bad(`gotchas.yaml does not parse: ${String(e.message).split("\n")[0]}`);
    }
  }

  const known = new Set(listed);
  for (const [id, e] of Object.entries(doc)) {
    if (!known.has(id)) {
      bad(`gotchas.yaml explains "${id}", which lesson.yaml does not list`);
      continue;
    }
    if (typeof e !== "object" || e === null) {
      bad(`"${id}": must have a title and a text`);
      continue;
    }
    const extra = Object.keys(e).filter((k) => k !== "title" && k !== "text" && k !== "demo");
    if (extra.length) bad(`"${id}": unknown field ${extra.join(", ")}`);
    for (const f of ["title", "text"]) {
      const v = e[f];
      if (typeof v !== "string") {
        bad(`"${id}": ${f} must be a string`);
        continue;
      }
      if (v !== v.trim()) bad(`"${id}": ${f} has leading or trailing whitespace`);
      const [lo, hi] = LIMITS[f];
      if (v.trim().length < lo || v.trim().length > hi) bad(`"${id}": ${f} is ${v.trim().length} characters (allowed ${lo} to ${hi})`);
      if (f === "text" && !/[.!?)`"]$/.test(v.trim())) bad(`"${id}": text should end like a sentence`);
      if (f === "title" && /[.:]$/.test(v.trim())) bad(`"${id}": title should not end with punctuation`);
    }
    if (e.demo !== undefined) checkDemo(id, e.demo, bad, rel);
  }
  const have = listed.filter((id) => id in doc && typeof doc[id] === "object" && doc[id] !== null && typeof doc[id].text === "string");
  covered += have.length;
  if (have.length < listed.length) partial.push(`${rel} (${have.length}/${listed.length})`);
}

const norm = (t) => String(t ?? "").replace(/\r\n/g, "\n");
const firstLine = (t) => norm(t).split("\n").find((l) => l.trim()) ?? "";

/** What is wrong with this result, given what the snippet declares? (empty list: nothing) */
export function judge(kind, sn, sanitize, r) {
  if (sn.error !== undefined) {
    if (r.compiled) return [`it compiled, but a compile error containing "${sn.error}" was expected`];
    return norm(r.compileOutput).includes(sn.error) ? [] : [`the compile error does not contain "${sn.error}": ${firstLine(r.compileOutput)}`];
  }
  if (!r.compiled) return [`did not compile: ${firstLine(r.compileOutput)}`];
  const out = [];
  if (kind === "fix" && r.compileOutput.trim()) out.push(`the fix builds with diagnostics: ${firstLine(r.compileOutput)}`);
  if (sn.warns !== undefined && !norm(r.compileOutput).includes(sn.warns)) out.push(`no warning containing "${sn.warns}" (got: ${firstLine(r.compileOutput) || "none"})`);
  if (r.timedOut) out.push("it ran into the time limit");
  if (sn.out !== undefined && norm(r.stdout) !== norm(sn.out)) out.push(`stdout is ${JSON.stringify(norm(r.stdout))}, expected ${JSON.stringify(norm(sn.out))}`);
  if (sn.reports !== undefined && !norm(r.stderr).includes(sn.reports)) out.push(`the sanitizer report does not contain "${sn.reports}" (stderr: ${JSON.stringify(firstLine(r.stderr))})`);
  if (kind === "fix" && sanitize && norm(r.stderr).trim()) out.push(`the fix prints a report: ${firstLine(r.stderr)}`);
  if (sn.reports === undefined && r.exitCode !== 0 && !r.timedOut) out.push(`exit code ${r.exitCode}${r.signal ? ` (${r.signal})` : ""}`);
  return out;
}

async function runDemos(name, run, only) {
  let n = 0;
  for (const { rel, id, demo } of demos) {
    if (!only(demo)) continue;
    for (const kind of ["bad", "fix"]) {
      const flags = ["-std=c++20", "-Wall", "-Wextra", ...(demo.sanitize ? ["-fsanitize=address,undefined"] : [])];
      let r;
      try {
        r = await run({ mode: "run", userFiles: { "main.cpp": demo[kind].code }, testFiles: {}, flags, limits: { wallMs: 8000 } });
      } catch (e) {
        problems.push(`${rel}: "${id}" ${kind} [${name}]: the runner failed: ${e.message}`);
        continue;
      }
      n++;
      for (const m of judge(kind, demo[kind], demo.sanitize, r)) problems.push(`${rel}: "${id}" ${kind} [${name}]: ${m}`);
    }
  }
  console.log(`${name}: ran ${n} demo program(s)`);
}

if (runDocker) {
  const { runJob } = await import("./runner-client.mjs");
  await runDemos("docker", runJob, () => true);
}
if (runWasm) {
  const { createNodeRunner } = await import("../../wasm-runner/src/node-host.mjs");
  const runner = createNodeRunner();
  try {
    await runDemos("wasm", (req) => runner.run(req), (d) => !d.sanitize && !d.server);
  } finally {
    await runner.close();
  }
}

if (list) for (const p of partial) console.log(`  not fully explained: ${p}`);
for (const p of problems) console.log(`FAIL  ${p}`);
const missing = ids - covered;
console.log(`${demos.length} runnable demo(s)`);
console.log(`${covered}/${ids} gotchas explained${missing ? `, ${missing} still to write in ${partial.length} lesson(s)` : ""}${problems.length ? `, ${problems.length} problem(s)` : ""}`);
process.exit(problems.length || (strict && missing) ? 1 : 0);
