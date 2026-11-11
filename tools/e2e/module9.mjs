// UI checks for Module 9 (Concurrency): every lesson is solved through the real UI with its reference solution, finishing them all completes
// the module, and the test-out completes it on its own. Also checks what is new in this module: a data race that ThreadSanitizer reports is
// explained in plain language (with the line of the OTHER access), and a run that the time limit kills is never shown as "All tests passed".
// Needs the Docker runner (threads and ThreadSanitizer).
//   usage: node module9.mjs        env: APP_URL (default http://localhost:5173), SHOTS (screenshot dir)
import { createSuite } from "./lib.mjs";

const t = createSuite({ track: "09-concurrency/" });
const { APP, check, shot, open, setFile, runTests, solutionFiles, solve, newPage } = t;
const LESSONS = [
  ["core.concurrency.threads", "01-threads-and-jthread", "Threads and jthread"],
  ["core.concurrency.data-races", "02-data-races-and-mutexes", "Data races and mutexes"],
  ["core.concurrency.condition-variables", "03-condition-variables-and-queues", "Condition variables and queues"],
  ["core.concurrency.atomics", "04-atomics-and-memory-ordering", "Atomics and memory ordering"],
  ["core.concurrency.futures", "05-futures-async-and-thread-pools", "Futures, async and thread pools"],
  ["core.concurrency.coroutines", "06-coroutines", "Coroutines"],
  ["core.concurrency.reading-bugs", "07-debug-reading-concurrency-bugs", "Reading concurrency bugs"],
  ["core.concurrency.capstone-job-scheduler", "08-capstone-job-scheduler", "Job Scheduler"],
];

await t.launch();

try {
  const page = await newPage();

  await page.goto(`${APP}/#/`);
  const module9 = page.locator("section.module", { hasText: "Concurrency" });
  await module9.waitFor();
  check((await module9.locator(".lesson-list a").count()) === 1 + LESSONS.length, "Module 9 lists the test-out, six lessons, the debug challenge and the capstone");

  // ---- a data race, as the learner meets it: plain Run on the starter of the debug challenge ----
  await open(page, "core.concurrency.reading-bugs");
  await page.getByRole("button", { name: /^▶ Run$/ }).click();
  await page.locator(".runtime-card").first().waitFor({ timeout: 40000 });
  const card = (await page.locator(".runtime-card").first().textContent()) ?? "";
  check(/same memory without coordination|data race/i.test(card), "ThreadSanitizer's data race is explained in plain language");
  check(/the other thread/i.test(card), "the card shows where the other access happened");
  check((await page.locator(".runtime-card a.learn-more").count()) === 1, "the card links to the lesson about data races");
  await shot(page, "m9-race-card");

  // ---- a run that the time limit kills must not look like a success ----
  await open(page, "core.concurrency.condition-variables");
  await setFile(page, "queue.hpp", (await solutionFiles("03-condition-variables-and-queues"))["queue.hpp"].replace("not_empty_.wait(lock, [this] { return closed_ || !items_.empty(); });", "not_empty_.wait(lock);"));
  const killed = await runTests(page, /stopped by the time limit|did not compile|tests passed|failed/, 120000);
  check(!/All tests passed/.test(killed), `a lost wake-up does not show as a pass (${killed.slice(0, 90)})`);
  await shot(page, "m9-killed-run");

  // ---- every lesson, solved through the UI ----
  for (const [id, dir, label] of LESSONS) await solve(page, id, dir, label);
  await shot(page, "m9-capstone-pass");

  // ---------------- the module is complete by lessons alone ----------------
  await page.goto(`${APP}/#/`);
  const head = (await module9.locator("h2").textContent()) ?? "";
  check(/module complete/.test(head) && !/tested out/.test(head), `Module 9 is complete by finishing the lessons (${head.trim()})`);
  await shot(page, "m9-home");
  await page.close();

  // ---------------- the test-out completes it by itself (fresh storage) ----------------
  const fresh = await newPage();
  await open(fresh, "core.concurrency.test-out");
  let b = await runTests(fresh, /did not compile|tests passed|failed|time limit/);
  check(!/All tests passed/.test(b), `test-out: the stub does not pass (${b.slice(0, 80)})`);
  await setFile(fresh, "parallel_sum.hpp", (await solutionFiles("00-test-out-parallel-sum"))["parallel_sum.hpp"]);
  b = await runTests(fresh, /All tests passed/);
  check(/All tests passed/.test(b), `test-out: ${b}`);
  await fresh.goto(`${APP}/#/`);
  const m9 = fresh.locator("section.module", { hasText: "Concurrency" });
  check(((await m9.locator("h2 .badge.ok").textContent()) ?? "").includes("tested out"), "passing the test-out completes the module (tested out)");
  await shot(fresh, "m9-testout-home");
  await fresh.close();
} catch (e) {
  t.crashed(e);
}

await t.finish();
