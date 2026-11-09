// Verifies content/diagnostics/friendly.yaml against REAL Clang output:
//   - every rule id is unique and every regex compiles
//   - every rule has a `trigger` program, and compiling it makes the runner produce at least one
//     diagnostic whose FIRST matching rule is this rule (so patterns neither miss nor get shadowed)
//   - every `lesson` link points at a lesson that exists
// usage: node friendly-check.mjs        env: RUNNER_URL (default http://localhost:8080)
import { readFile, readdir } from "node:fs/promises";
import { join, resolve } from "node:path";
import { fileURLToPath } from "node:url";
import { parse } from "yaml";
import { runJob } from "./runner-client.mjs";
import { compileRules, matchRule } from "../../content/diagnostics/friendly-core.mjs";

const ROOT = resolve(fileURLToPath(new URL("../../content", import.meta.url)));

async function* walk(dir) {
  for (const e of await readdir(dir, { withFileTypes: true })) {
    const p = join(dir, e.name);
    if (e.isDirectory()) yield* walk(p);
    else yield p;
  }
}

const raw = parse(await readFile(join(ROOT, "diagnostics", "friendly.yaml"), "utf8"));
const rules = compileRules(raw); // throws on a bad regex, naming the rule

const lessonIds = new Set();
for await (const p of walk(join(ROOT, "tracks"))) {
  if (p.endsWith("lesson.yaml")) lessonIds.add(parse(await readFile(p, "utf8")).id);
}

const errors = [];
const seen = new Set();
for (const r of raw) {
  if (seen.has(r.id)) errors.push(`${r.id}: duplicate id`);
  seen.add(r.id);
  if (r.lesson && !lessonIds.has(r.lesson)) errors.push(`${r.id}: lesson "${r.lesson}" does not exist`);
  if (!r.friendly?.trim()) errors.push(`${r.id}: empty friendly text`);
  if (!r.trigger) {
    errors.push(`${r.id}: no trigger program (every rule must be verifiable against real Clang)`);
    continue;
  }
  const t = typeof r.trigger === "string" ? { files: { "main.cpp": r.trigger } } : r.trigger;
  let out;
  try {
    out = await runJob({
      mode: t.testFiles ? "test" : "run",
      userFiles: t.files,
      testFiles: t.testFiles ?? {},
      flags: t.flags ?? ["-std=c++20", "-Wall", "-Wextra"],
    });
  } catch (e) {
    errors.push(`${r.id}: ${e.message}`);
    continue;
  }
  const fired = out.diagnostics.map((d) => matchRule(rules, d)?.ruleId).filter(Boolean);
  if (!fired.includes(r.id)) {
    const msgs = out.diagnostics.map((d) => `[${d.severity}] ${d.file}: ${d.message}`).join("\n        ");
    errors.push(`${r.id}: its trigger did not make this rule fire. Rules that fired: [${[...new Set(fired)].join(", ")}]\n        ${msgs || out.compileOutput}`);
  } else {
    console.log(`ok    ${r.id}`);
  }
}

if (errors.length) {
  console.log("");
  for (const e of errors) console.log(`FAIL  ${e}`);
}
console.log(`\n${raw.length - errors.length}/${raw.length} friendly rules verified against real Clang output`);
process.exit(errors.length ? 1 : 0);
