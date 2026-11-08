// UI checks for Module 4 (Ownership and RAII): every lesson is solved through the real UI with its reference
// solution, finishing them all completes the module, and the test-out completes it on its own.
//   usage: node module4.mjs        env: APP_URL (default http://localhost:5173), SHOTS (screenshot dir)
import { createSuite } from "./lib.mjs";

const t = createSuite({ track: "04-ownership-and-raii/" });
const { APP, check, shot, open, setCode, setFile, runTests, solutionFiles, solve, newPage } = t;
const LESSONS = [
  ["core.ownership.raii", "01-destructors-and-raii", "Destructors and RAII"],
  ["core.ownership.rule-of-three", "02-copying-and-the-rule-of-three", "Rule of three"],
  ["core.ownership.rule-of-five", "03-moving-and-the-rule-of-five", "Rule of five"],
  ["core.ownership.unique-ptr", "04-unique-ptr-and-the-rule-of-zero", "unique_ptr"],
  ["core.ownership.shared-ptr", "05-shared-ptr-and-weak-ptr", "shared_ptr"],
  ["core.ownership.find-the-bug", "06-find-the-lifetime-bug", "Lifetime bugs"],
  ["core.ownership.capstone-vector", "07-capstone-int-vector", "Capstone"],
];

await t.launch();

try {
  const page = await newPage();

  await page.goto(`${APP}/#/`);
  const module4 = page.locator("section.module", { hasText: "Ownership and RAII" });
  await module4.waitFor();
  check((await module4.locator(".lesson-list a").count()) === 1 + LESSONS.length, "Module 4 lists the test-out, five lessons, the debug challenge and the capstone");
  check(!/More lessons are on the way/.test((await module4.textContent()) ?? ""), "the 'more lessons are on the way' note is gone");

  // ---------------- a sanitizer report, for real: Run on the starter of the lifetime-bug challenge ----------------
  await open(page, "core.ownership.find-the-bug");
  await page.getByRole("button", { name: /^▶ Run$/ }).click();
  await page.locator(".runtime-card").first().waitFor({ timeout: 60000 });
  const card = (await page.locator(".runtime-card").first().textContent()) ?? "";
  check(/Used a local variable after its function returned/.test(card), "lifetime bugs: Run shows the sanitizer's explanation for the first bug");
  check(/main\.cpp:\d+/.test(card), "lifetime bugs: and the line where it happened");
  check((await page.locator(".runtime-card details.raw").count()) === 1, "lifetime bugs: the raw report is one click away");
  await shot(page, "m4-sanitizer-card");

  // ---------------- the starter of the rule-of-three lesson crashes with a double free report ----------------
  await open(page, "core.ownership.rule-of-three");
  await page.getByRole("button", { name: /^▶ Run$/ }).click();
  const second = page.locator(".runtime-card", { hasText: /freed|never freed/i }).first();
  await second.waitFor({ timeout: 60000 });
  check(/freed/i.test((await second.textContent()) ?? ""), "rule of three: Run on the starter reports the memory error in plain language");
  await shot(page, "m4-rule-of-three-starter");

  for (const [id, dir, label] of LESSONS) await solve(page, id, dir, label);
  await shot(page, "m4-capstone-pass");

  // ---------------- the module is complete by lessons alone ----------------
  await page.goto(`${APP}/#/`);
  const head = (await module4.locator("h2").textContent()) ?? "";
  check(/module complete/.test(head) && !/tested out/.test(head), `Module 4 is complete by finishing the lessons (${head.trim()})`);
  await shot(page, "m4-home");
  await page.close();

  // ---------------- the test-out completes it by itself (fresh storage) ----------------
  const fresh = await newPage();
  await open(fresh, "core.ownership.test-out");
  let b = await runTests(fresh, /did not compile|tests passed|failed/);
  check(!/All tests passed/.test(b), `test-out: the initial code does not pass (${b})`);
  await setFile(fresh, "buffer.hpp", (await solutionFiles("00-test-out-buffer"))["buffer.hpp"]);
  b = await runTests(fresh, /All tests passed/);
  check(/All tests passed/.test(b), `test-out: ${b}`);
  await fresh.goto(`${APP}/#/`);
  const m4 = fresh.locator("section.module", { hasText: "Ownership and RAII" });
  check(((await m4.locator("h2 .badge.ok").textContent()) ?? "").includes("tested out"), "passing the test-out completes the module (tested out)");
  await shot(fresh, "m4-testout-home");
  await fresh.close();
} catch (e) {
  t.crashed(e);
}

await t.finish();
