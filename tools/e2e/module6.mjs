// UI checks for Module 6 (The Standard Library): every lesson is solved through the real UI with its reference
// solution, finishing them all completes the module, and the test-out completes it on its own.
//   usage: node module6.mjs        env: APP_URL (default http://localhost:5173), SHOTS (screenshot dir)
import { createSuite } from "./lib.mjs";

const t = createSuite({ track: "06-standard-library/" });
const { APP, check, shot, open, setFile, runTests, solutionFiles, solve, newPage } = t;
const LESSONS = [
  ["core.stdlib.containers", "01-containers-tour", "Containers"],
  ["core.stdlib.iterators", "02-iterators-and-ranges", "Iterators and ranges"],
  ["core.stdlib.algorithms", "03-algorithms", "Algorithms"],
  ["core.stdlib.lambdas", "04-lambdas-and-callables", "Lambdas"],
  ["core.stdlib.vocabulary", "05-vocabulary-types", "Vocabulary types"],
  ["core.stdlib.text", "06-strings-and-parsing", "Strings and parsing"],
  ["core.stdlib.time-random", "07-time-random-numerics", "Time, random, numerics"],
  ["core.stdlib.capstone-log-analyzer", "08-capstone-log-analyzer", "Log Analyzer"],
];

await t.launch();

try {
  const page = await newPage();

  await page.goto(`${APP}/#/`);
  const module6 = page.locator("section.module", { hasText: "The Standard Library" });
  await module6.waitFor();
  check((await module6.locator(".lesson-list a").count()) === 1 + LESSONS.length, "Module 6 lists the test-out, seven lessons and the capstone");
  check(!/More lessons are on the way/.test((await module6.textContent()) ?? ""), "the 'more lessons are on the way' note is gone");

  for (const [id, dir, label] of LESSONS) await solve(page, id, dir, label);
  await shot(page, "m6-capstone-pass");

  // ---------------- the module is complete by lessons alone ----------------
  await page.goto(`${APP}/#/`);
  const head = (await module6.locator("h2").textContent()) ?? "";
  check(/module complete/.test(head) && !/tested out/.test(head), `Module 6 is complete by finishing the lessons (${head.trim()})`);
  await shot(page, "m6-home");
  await page.close();

  // ---------------- the test-out completes it by itself (fresh storage) ----------------
  const fresh = await newPage();
  await open(fresh, "core.stdlib.test-out");
  let b = await runTests(fresh, /did not compile|tests passed|failed/);
  check(!/All tests passed/.test(b), `test-out: the initial code does not pass (${b})`);
  await setFile(fresh, "top_k.hpp", (await solutionFiles("00-test-out-top-k"))["top_k.hpp"]);
  b = await runTests(fresh, /All tests passed/);
  check(/All tests passed/.test(b), `test-out: ${b}`);
  await fresh.goto(`${APP}/#/`);
  const m6 = fresh.locator("section.module", { hasText: "The Standard Library" });
  check(((await m6.locator("h2 .badge.ok").textContent()) ?? "").includes("tested out"), "passing the test-out completes the module (tested out)");
  await shot(fresh, "m6-testout-home");
  await fresh.close();
} catch (e) {
  t.crashed(e);
}

await t.finish();
