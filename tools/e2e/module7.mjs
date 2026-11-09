// UI checks for Module 7 (Inheritance and Polymorphism): every lesson is solved through the real UI with its reference
// solution, finishing them all completes the module, and the test-out completes it on its own.
//   usage: node module7.mjs        env: APP_URL (default http://localhost:5173), SHOTS (screenshot dir)
import { createSuite } from "./lib.mjs";

const t = createSuite({ track: "07-inheritance-and-polymorphism/" });
const { APP, check, shot, open, setFile, runTests, solutionFiles, solve, newPage } = t;
const LESSONS = [
  ["core.inherit.basics", "01-inheritance-basics", "Inheritance basics"],
  ["core.inherit.virtual", "02-virtual-functions", "Virtual functions"],
  ["core.inherit.interfaces", "03-abstract-classes-and-interfaces", "Abstract classes and interfaces"],
  ["core.inherit.composition", "04-composition-over-inheritance", "Composition over inheritance"],
  ["core.inherit.alternatives", "05-polymorphism-alternatives", "Runtime polymorphism vs alternatives"],
  ["core.inherit.reading-bugs", "06-reading-polymorphism-bugs", "Reading polymorphism bugs"],
  ["core.inherit.capstone-plugin-pipeline", "07-capstone-plugin-pipeline", "Plugin Pipeline"],
];

await t.launch();

try {
  const page = await newPage();

  await page.goto(`${APP}/#/`);
  const module7 = page.locator("section.module", { hasText: "Inheritance and Polymorphism" });
  await module7.waitFor();
  check((await module7.locator(".lesson-list a").count()) === 1 + LESSONS.length, "Module 7 lists the test-out, six lessons and the capstone");
  check(!/More lessons are on the way/.test((await module7.textContent()) ?? ""), "the 'more lessons are on the way' note is gone");

  for (const [id, dir, label] of LESSONS) await solve(page, id, dir, label);
  await shot(page, "m7-capstone-pass");

  // ---------------- the module is complete by lessons alone ----------------
  await page.goto(`${APP}/#/`);
  const head = (await module7.locator("h2").textContent()) ?? "";
  check(/module complete/.test(head) && !/tested out/.test(head), `Module 7 is complete by finishing the lessons (${head.trim()})`);
  await shot(page, "m7-home");
  await page.close();

  // ---------------- the test-out completes it by itself (fresh storage) ----------------
  const fresh = await newPage();
  await open(fresh, "core.inherit.test-out");
  let b = await runTests(fresh, /did not compile|tests passed|failed/);
  check(!/All tests passed/.test(b), `test-out: the initial code does not pass (${b})`);
  await setFile(fresh, "shapes.hpp", (await solutionFiles("00-test-out-shape-area"))["shapes.hpp"]);
  b = await runTests(fresh, /All tests passed/);
  check(/All tests passed/.test(b), `test-out: ${b}`);
  await fresh.goto(`${APP}/#/`);
  const m7 = fresh.locator("section.module", { hasText: "Inheritance and Polymorphism" });
  check(((await m7.locator("h2 .badge.ok").textContent()) ?? "").includes("tested out"), "passing the test-out completes the module (tested out)");
  await shot(fresh, "m7-testout-home");
  await fresh.close();
} catch (e) {
  t.crashed(e);
}

await t.finish();
