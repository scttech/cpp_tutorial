// Verifies content/diagnostics/runtime.yaml against the REAL sandbox (sanitizers run for real):
//   - every rule id is unique and every regex compiles
//   - every rule has a `trigger` program; running it with -fsanitize makes the runner produce a report whose FIRST
//     matching rule is this rule (so patterns neither miss nor get shadowed), and the friendly text has no unfilled gaps
//   - every `lesson` link points at a lesson that exists
// usage: node runtime-check.mjs        env: RUNNER_URL (default http://localhost:8080)
import { readFile } from "node:fs/promises";
import { join } from "node:path";
import { parse } from "yaml";
import { describeReport, compileRuntimeRules } from "../../content/diagnostics/runtime-core.mjs";
import { CONTENT_ROOT, findLessonDirs, loadLesson } from "./lessons.mjs";
import { runJob } from "./runner-client.mjs";

const raw = parse(await readFile(join(CONTENT_ROOT, "diagnostics", "runtime.yaml"), "utf8"));
const rules = compileRuntimeRules(raw); // throws on a bad regex, naming the rule

const lessonIds = new Set();
for (const dir of await findLessonDirs()) lessonIds.add((await loadLesson(dir)).lesson.id);

const errors = [];
const seen = new Set();
for (const r of raw) {
  if (seen.has(r.id)) errors.push(`${r.id}: duplicate id`);
  seen.add(r.id);
  if (r.lesson && !lessonIds.has(r.lesson)) errors.push(`${r.id}: lesson "${r.lesson}" does not exist`);
  if (!r.friendly?.trim() || !r.title?.trim()) errors.push(`${r.id}: empty title or friendly text`);
  if (!r.trigger) {
    errors.push(`${r.id}: no trigger program (every rule must be verifiable against the real sandbox)`);
    continue;
  }
  const t = typeof r.trigger === "string" ? { files: { "main.cpp": r.trigger } } : r.trigger;
  let out;
  try {
    out = await runJob({
      mode: t.testFiles ? "test" : "run",
      userFiles: t.files,
      testFiles: t.testFiles ?? {},
      flags: r.flags ?? ["-std=c++20", "-fsanitize=address,undefined"],
      limits: { memoryMb: 256 },
    });
  } catch (e) {
    errors.push(`${r.id}: ${e.message}`);
    continue;
  }
  if (!out.compiled) {
    errors.push(`${r.id}: its trigger does not compile:\n        ${out.compileOutput.split("\n").slice(0, 4).join("\n        ")}`);
    continue;
  }
  if (!out.report) {
    errors.push(`${r.id}: the trigger produced no sanitizer report (exit ${out.exitCode}). stderr: ${out.stderr.slice(0, 200)}`);
    continue;
  }
  const hit = describeReport(rules, out.report);
  if (hit?.ruleId !== r.id) {
    errors.push(`${r.id}: the trigger's report is "${out.report.summary}", which matched ${hit ? `rule "${hit.ruleId}"` : "no rule"}`);
    continue;
  }
  // The triggers are meant to give every placeholder a real value from the report (not the neutral fallback phrase).
  const need = { where: "location", freed: "freedAt", allocated: "allocatedAt", other: "previousAt", access: "access" };
  const missing = Object.entries(need).filter(([ph, field]) => r.friendly.includes(`{${ph}}`) && !out.report[field]).map(([ph]) => ph);
  if (missing.length) {
    errors.push(`${r.id}: the report has nothing for the placeholder(s) {${missing.join("}, {")}} (report: ${JSON.stringify(out.report).slice(0, 300)})`);
    continue;
  }
  console.log(`ok    ${r.id}  (${out.report.location ? `${out.report.location.file}:${out.report.location.line}` : "no location"})`);
}

if (errors.length) {
  console.log("");
  for (const e of errors) console.log(`FAIL  ${e}`);
}
console.log(`\n${raw.length - errors.length}/${raw.length} runtime rules verified against real sanitizer output`);
process.exit(errors.length ? 1 : 0);
