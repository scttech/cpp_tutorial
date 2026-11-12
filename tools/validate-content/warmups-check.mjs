// Checks the warm-up question bank, content/warmups/*.yaml: a list of multiple-choice questions.
//   - every file parses and is a list; every entry has a kebab-case id (unique across all files), a topic, a question,
//     2 to 5 different choices, an `answer` that is the index of a choice, and an explanation
//   - questions are not duplicated, and the right answer is not in the same position for (nearly) every question
//     (the app shuffles choices, but `ordered: true` questions keep the order written)
// usage: node warmups-check.mjs
import { readFile, readdir } from "node:fs/promises";
import { join, resolve } from "node:path";
import { fileURLToPath } from "node:url";
import { parse } from "yaml";

const DIR = resolve(fileURLToPath(new URL("../../content/warmups", import.meta.url)));
const problems = [];
const ids = new Map();
const questions = new Map();
const topics = new Map();
let total = 0;
const answerPositions = [0, 0, 0, 0, 0];

for (const f of (await readdir(DIR)).filter((n) => n.endsWith(".yaml")).sort()) {
  let doc;
  try {
    doc = parse(await readFile(join(DIR, f), "utf8"));
  } catch (e) {
    problems.push(`${f}: does not parse: ${String(e.message).split("\n")[0]}`);
    continue;
  }
  if (!Array.isArray(doc)) {
    problems.push(`${f}: must be a list of questions`);
    continue;
  }
  doc.forEach((q, i) => {
    const at = (m) => problems.push(`${f} #${i + 1}${q && q.id ? ` (${q.id})` : ""}: ${m}`);
    if (typeof q !== "object" || q === null || Array.isArray(q)) return at("not a mapping");
    total++;
    const known = ["id", "topic", "question", "code", "choices", "answer", "explain", "ordered"];
    const extra = Object.keys(q).filter((k) => !known.includes(k));
    if (extra.length) at(`unknown field ${extra.join(", ")}`);
    if (typeof q.id !== "string" || !/^[a-z0-9]+(-[a-z0-9]+)*$/.test(q.id)) at("id must be kebab-case");
    else if (ids.has(q.id)) at(`id also used in ${ids.get(q.id)}`);
    else ids.set(q.id, f);
    for (const [k, min] of [["topic", 2], ["question", 8], ["explain", 20]]) {
      if (typeof q[k] !== "string" || q[k].trim().length < min) at(`${k} must be a string of at least ${min} characters`);
    }
    if (typeof q.explain === "string" && q.explain.length > 600) at("explain is too long (600 characters at most)");
    if (typeof q.question === "string") {
      const key = q.question.trim().toLowerCase();
      if (questions.has(key) && !q.code) at(`the same question as ${questions.get(key)}`);
      else questions.set(key, q.id ?? `${f} #${i + 1}`);
    }
    if (q.code !== undefined && (typeof q.code !== "string" || q.code.split("\n").length > 24)) at("code must be a string of at most 24 lines");
    const c = q.choices;
    if (!Array.isArray(c) || c.length < 2 || c.length > 5 || c.some((x) => typeof x !== "string" || x.trim() === "")) at("choices must be 2 to 5 non-empty strings");
    else {
      if (new Set(c.map((x) => x.trim())).size !== c.length) at("choices must be different from each other");
      if (c.some((x) => x.length > 200)) at("a choice is too long (200 characters at most)");
      if (!Number.isInteger(q.answer) || q.answer < 0 || q.answer >= c.length) at("answer must be the index of a choice");
      else if (q.answer < answerPositions.length) answerPositions[q.answer]++;
    }
    topics.set(q.topic, (topics.get(q.topic) ?? 0) + 1);
  });
}
// the written order matters only for `ordered` questions, but a bank where the answer is nearly always first is a smell
const maxShare = Math.max(...answerPositions) / Math.max(1, total);
if (total >= 20 && maxShare > 0.7) problems.push(`the right answer is at the same position in ${Math.round(maxShare * 100)}% of the questions: vary it`);

for (const p of problems) console.log(`FAIL  ${p}`);
console.log(`${total} warm-up question(s) in ${topics.size} topic(s): ${[...topics].map(([t, n]) => `${t} ${n}`).join(", ")}${problems.length ? `, ${problems.length} problem(s)` : ""}`);
process.exit(problems.length ? 1 : 0);
