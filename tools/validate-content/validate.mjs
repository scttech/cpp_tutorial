// Validates every lesson under content/ against the live runner (spec section 3.4):
//   1. starter compiles (unless starterBroken) and FAILS at least one test
//   2. solution compiles with ZERO diagnostics and passes ALL tests
//   3. every test has a name and a tier the lesson declares; hints.yaml is well-formed
// usage: node validate.mjs [lesson-id-substring]    env: RUNNER_URL (default http://localhost:8080)
import { readdir, readFile, stat } from "node:fs/promises";
import { join, relative, resolve, sep } from "node:path";
import { fileURLToPath } from "node:url";
import { parse } from "yaml";
import { runJob } from "./runner-client.mjs";

const ROOT = resolve(fileURLToPath(new URL("../../content", import.meta.url)));
const filter = process.argv[2];

async function* walk(dir) {
  for (const e of await readdir(dir, { withFileTypes: true })) {
    const p = join(dir, e.name);
    if (e.isDirectory()) yield* walk(p);
    else yield p;
  }
}

async function readTree(dir) {
  const files = {};
  try { await stat(dir); } catch { return files; }
  for await (const p of walk(dir)) files[relative(dir, p).split(sep).join("/")] = await readFile(p, "utf8");
  return files;
}

async function runTests(lesson, userFiles, testFiles) {
  return runJob({
    mode: "test",
    userFiles,
    testFiles,
    flags: lesson.flags ?? ["-std=c++20", "-Wall", "-Wextra"],
    limits: lesson.limits ?? {},
  });
}

async function validate(dir) {
  const errors = [];
  const lesson = parse(await readFile(join(dir, "lesson.yaml"), "utf8"));
  for (const k of ["id", "title", "module", "type", "files", "tiers"]) if (!(k in lesson)) errors.push(`lesson.yaml: missing "${k}"`);
  const declared = Object.entries(lesson.tiers ?? {}).filter(([, v]) => v).map(([k]) => k);

  try {
    const hints = parse(await readFile(join(dir, "hints.yaml"), "utf8"));
    const levels = (hints?.ladder ?? []).map((h) => h.level);
    if (levels.join() !== "1,2,3,4") errors.push(`hints.yaml: expected levels 1,2,3,4 got [${levels}]`);
  } catch { errors.push("hints.yaml: missing or unreadable"); }
  for (const f of ["lesson.md", "fast.md"]) {
    try { await stat(join(dir, f)); } catch { errors.push(`${f}: missing`); }
  }

  const [starter, solution, tests] = await Promise.all(["starter", "solution", "tests"].map((d) => readTree(join(dir, d))));
  // Files the learner is given but may not edit are supplied alongside every solution and mutant.
  const starterReadonly = Object.fromEntries((lesson.files?.readonly ?? []).filter((f) => f in starter).map((f) => [f, starter[f]]));
  if (!Object.keys(tests).length) errors.push("tests/: no test files");
  if (errors.length) return { lesson, errors };

  // 1. solution
  const sol = await runTests(lesson, { ...starterReadonly, ...solution }, tests);
  if (!sol.compiled) errors.push(`solution does not compile:\n${sol.compileOutput}`);
  else {
    if (sol.compileOutput.trim()) errors.push(`solution compiles with diagnostics (must be zero):\n${sol.compileOutput}`);
    if (!sol.tests.length) errors.push("solution produced no test results (did the harness run?)");
    for (const t of sol.tests) {
      if (!t.name) errors.push("a test has no name");
      if (!t.tier || !declared.includes(t.tier)) errors.push(`test "${t.name}": tier "${t.tier}" not declared in lesson.yaml`);
      if (t.status !== "pass") errors.push(`solution fails test "${t.name}" [${t.status}]: ${t.message ?? ""}`);
      if (t.status === "fail" && !t.message) errors.push(`test "${t.name}" fails without a message`);
    }
    for (const tier of declared) if (!sol.tests.some((t) => t.tier === tier)) errors.push(`declared tier "${tier}" has no tests`);
  }

  // 2. starter
  const st = await runTests(lesson, starter, tests);
  if (!st.compiled && !lesson.starterBroken) errors.push(`starter does not compile (set starterBroken: true if intended):\n${st.compileOutput}`);
  if (st.compiled && st.tests.length && st.tests.every((t) => t.status === "pass")) errors.push("starter already passes every test");
  if (st.compiled && !st.tests.length) errors.push("starter produced no test results");

  // 3. mutants: plausible WRONG solutions (mutants/<name>/...) must each fail at least one test,
  //    proving the tests actually catch the mistakes the lesson warns about.
  const mutants = {};
  try {
    for (const m of await readdir(join(dir, "mutants"), { withFileTypes: true })) {
      if (m.isDirectory()) mutants[m.name] = await readTree(join(dir, "mutants", m.name));
    }
  } catch { /* no mutants directory */ }
  for (const [name, files] of Object.entries(mutants)) {
    // A file named EXPECT_COMPILE_FAIL marks a mutant that must be rejected by the build itself
    // (e.g. a missing include guard or a definition in a header causing a link error).
    const expectBuildFail = "EXPECT_COMPILE_FAIL" in files;
    delete files.EXPECT_COMPILE_FAIL;
    const r = await runTests(lesson, { ...starterReadonly, ...files }, tests);
    if (expectBuildFail) {
      if (r.compiled) errors.push(`mutant "${name}" was expected to fail to build but compiled`);
    } else if (!r.compiled) errors.push(`mutant "${name}" does not compile:
${r.compileOutput}`);
    else if (r.tests.every((t) => t.status === "pass")) errors.push(`mutant "${name}" passes every test (the tests miss this mistake)`);
  }
  return { lesson, errors, mutantCount: Object.keys(mutants).length };
}

const lessonDirs = [];
for await (const p of walk(ROOT)) if (p.endsWith(`${sep}lesson.yaml`)) lessonDirs.push(resolve(p, ".."));
lessonDirs.sort();

let failed = 0, checked = 0;
for (const dir of lessonDirs) {
  const rel = relative(ROOT, dir).split(sep).join("/");
  if (filter && !rel.includes(filter)) continue;
  checked++;
  try {
    const { lesson, errors, mutantCount } = await validate(dir);
    if (errors.length) { failed++; console.log(`FAIL  ${lesson?.id ?? rel}`); errors.forEach((e) => console.log("      - " + e.replaceAll("\n", "\n        "))); }
    else console.log(`ok    ${lesson.id}${mutantCount ? `  (${mutantCount} mutants caught)` : ""}`);
  } catch (e) { failed++; console.log(`FAIL  ${rel}\n      - ${e.message}`); }
}
console.log(`\n${checked - failed}/${checked} lessons valid`);
process.exit(failed || !checked ? 1 : 0);
