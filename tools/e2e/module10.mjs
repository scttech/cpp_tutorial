// UI checks for Module 10 (Performance): every lesson is solved through the real UI with its reference solution, finishing them all completes
// the module, and the test-out completes it on its own. The test-out and the capstone start from a program that is CORRECT but slow, so their
// starter passes Works and Robust and must still not count as finished (the Fast tier is what is missing).
//   usage: node module10.mjs        env: APP_URL (default http://localhost:5173), SHOTS (screenshot dir)
import { createSuite } from "./lib.mjs";

const t = createSuite({ track: "10-performance/" });
const { APP, check, shot, open, setFile, runTests, solutionFiles, solve, newPage } = t;
const LESSONS = [
  ["core.performance.measuring", "01-measuring-performance", "Measuring performance"],
  ["core.performance.layout", "02-memory-layout-and-caches", "Memory layout and caches"],
  ["core.performance.allocators", "03-allocation-costs-and-arenas", "Allocation costs and arenas"],
  ["core.performance.constexpr", "04-compile-time-computation", "Compile-time computation"],
  ["core.performance.tools", "05-performance-tools", "Performance tools"],
  ["core.performance.reading-slowness", "06-debug-reading-performance-problems", "Reading performance problems"],
  ["core.performance.capstone-optimize-the-pipeline", "07-capstone-optimize-the-pipeline", "Optimize the Pipeline"],
];

await t.launch();

try {
  const page = await newPage();

  await page.goto(`${APP}/#/`);
  const module10 = page.locator("section.module").filter({ has: page.locator("h2", { hasText: "Performance" }) });
  await module10.waitFor();
  check((await module10.locator(".lesson-list a").count()) === 1 + LESSONS.length, "Module 10 lists the test-out, five lessons, the debug challenge and the capstone");

  // ---- a correct but slow program: Works and Robust pass, the Fast tier does not, so it is not finished ----
  await open(page, "core.performance.capstone-optimize-the-pipeline");
  const slow = await runTests(page, /did not compile|tests passed|failed|time limit/);
  check(!/All tests passed/.test(slow), `the slow starter of the capstone does not finish the lesson (${slow.slice(0, 90)})`);
  await shot(page, "m10-capstone-slow");

  // ---- every lesson, solved through the UI ----
  for (const [id, dir, label] of LESSONS) await solve(page, id, dir, label);
  await shot(page, "m10-capstone-pass");

  // ---------------- the module is complete by lessons alone ----------------
  await page.goto(`${APP}/#/`);
  const head = (await module10.locator("h2").textContent()) ?? "";
  check(/module complete/.test(head) && !/tested out/.test(head), `Module 10 is complete by finishing the lessons (${head.trim()})`);
  await shot(page, "m10-home");
  await page.close();

  // ---------------- the test-out completes it by itself (fresh storage) ----------------
  const fresh = await newPage();
  await open(fresh, "core.performance.test-out");
  let b = await runTests(fresh, /did not compile|tests passed|failed|time limit/);
  check(!/All tests passed/.test(b), `test-out: the slow starter does not pass (${b.slice(0, 80)})`);
  await setFile(fresh, "count_pairs.hpp", (await solutionFiles("00-test-out-count-pairs"))["count_pairs.hpp"]);
  b = await runTests(fresh, /All tests passed/);
  check(/All tests passed/.test(b), `test-out: ${b}`);
  await fresh.goto(`${APP}/#/`);
  const m10 = fresh.locator("section.module").filter({ has: fresh.locator("h2", { hasText: "Performance" }) });
  check(((await m10.locator("h2 .badge.ok").textContent()) ?? "").includes("tested out"), "passing the test-out completes the module (tested out)");
  await shot(fresh, "m10-testout-home");
  await fresh.close();
} catch (e) {
  t.crashed(e);
}

await t.finish();
