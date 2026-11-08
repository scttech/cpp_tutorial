// UI checks for Module 5 (Generic Programming): every lesson is solved through the real UI with its reference
// solution, finishing them all completes the module, and the test-out completes it on its own.
//   usage: node module5.mjs        env: APP_URL (default http://localhost:5173), SHOTS (screenshot dir)
import { createSuite } from "./lib.mjs";

const t = createSuite({ track: "05-generic-programming/" });
const { APP, check, shot, open, setFile, runTests, solutionFiles, solve, newPage } = t;
const LESSONS = [
  ["core.generics.function-templates", "01-function-templates", "Function templates"],
  ["core.generics.class-templates", "02-class-templates", "Class templates"],
  ["core.generics.specialization", "03-specialization-and-if-constexpr", "Specialization"],
  ["core.generics.concepts", "04-concepts", "Concepts"],
  ["core.generics.variadic", "05-variadic-templates", "Variadic templates"],
  ["core.generics.type-traits", "06-auto-decltype-and-type-traits", "Type traits"],
  ["core.generics.reading-errors", "07-reading-template-errors", "Reading template errors"],
  ["core.generics.capstone-ring-buffer", "08-capstone-ring-buffer", "RingBuffer"],
  ["core.generics.capstone-static-vector", "09-capstone-static-vector", "StaticVector"],
];

await t.launch();

try {
  const page = await newPage();

  await page.goto(`${APP}/#/`);
  const module5 = page.locator("section.module", { hasText: "Generic Programming" });
  await module5.waitFor();
  check((await module5.locator(".lesson-list a").count()) === 1 + LESSONS.length, "Module 5 lists the test-out, six lessons, the debug challenge and the two capstones");
  check(!/More lessons are on the way/.test((await module5.textContent()) ?? ""), "the 'more lessons are on the way' note is gone");

  // ---------------- the debug challenge does not compile: the template errors come with friendly explanations ----------------
  await open(page, "core.generics.reading-errors");
  await page.getByRole("button", { name: /^▶ Run$/ }).click();
  await page.locator(".start-here").waitFor({ timeout: 60000 });
  const start = (await page.locator(".start-here").textContent()) ?? "";
  check(/Start with the first error/.test(start) && start.length > 60, "reading errors: the Run on the broken starter shows a 'start with the first error' explanation");
  await page.getByRole("tab", { name: /Problems/ }).click();
  const problems = (await page.locator(".problems").textContent().catch(() => "")) ?? "";
  const texts = (await page.locator(".ptext").allTextContents()).join("\n");
  check(/ONE type|inside the standard library|INSIDE a template|COMPILE time|typename/i.test(texts || problems), "reading errors: the Problems tab explains the template errors in plain language");
  await shot(page, "m5-template-errors");

  for (const [id, dir, label] of LESSONS) await solve(page, id, dir, label);
  await shot(page, "m5-capstone-pass");

  // ---------------- the module is complete by lessons alone ----------------
  await page.goto(`${APP}/#/`);
  const head = (await module5.locator("h2").textContent()) ?? "";
  check(/module complete/.test(head) && !/tested out/.test(head), `Module 5 is complete by finishing the lessons (${head.trim()})`);
  await shot(page, "m5-home");
  await page.close();

  // ---------------- the test-out completes it by itself (fresh storage) ----------------
  const fresh = await newPage();
  await open(fresh, "core.generics.test-out");
  let b = await runTests(fresh, /did not compile|tests passed|failed/);
  check(!/All tests passed/.test(b), `test-out: the initial code does not pass (${b})`);
  await setFile(fresh, "max_by.hpp", (await solutionFiles("00-test-out-max-by"))["max_by.hpp"]);
  b = await runTests(fresh, /All tests passed/);
  check(/All tests passed/.test(b), `test-out: ${b}`);
  await fresh.goto(`${APP}/#/`);
  const m5 = fresh.locator("section.module", { hasText: "Generic Programming" });
  check(((await m5.locator("h2 .badge.ok").textContent()) ?? "").includes("tested out"), "passing the test-out completes the module (tested out)");
  await shot(fresh, "m5-testout-home");
  await fresh.close();
} catch (e) {
  t.crashed(e);
}

await t.finish();
