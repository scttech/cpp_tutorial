// UI checks for Module 8 (Testing and Tooling): every lesson is solved through the real UI with its reference solution, finishing them all
// completes the module, and the test-out completes it on its own. Also checks what is new in this module: the graded-tests lessons name the
// bugs that the learner's tests missed, and the warning lesson explains its -Werror errors in plain language.
// Needs the Docker runner (GoogleTest, Catch2, clang-tidy, cppcheck and clang-format live in its image).
//   usage: node module8.mjs        env: APP_URL (default http://localhost:5173), SHOTS (screenshot dir)
import { createSuite } from "./lib.mjs";

const t = createSuite({ track: "08-testing-and-tooling/" });
const { APP, check, shot, open, setFile, runTests, solutionFiles, solve, newPage } = t;
const LESSONS = [
  ["core.testing.why-test", "01-why-test-and-what-a-test-is", "Why test"],
  ["core.testing.googletest", "02-googletest", "GoogleTest"],
  ["core.testing.catch2", "03-catch2", "Catch2"],
  ["core.testing.mocks", "04-test-doubles-and-googlemock", "Test doubles and GoogleMock"],
  ["core.testing.warnings", "05-compiler-warnings", "Compiler warnings"],
  ["core.testing.static-analysis", "06-static-analysis", "Static analysis"],
  ["core.testing.fuzzing", "07-sanitizers-fuzzing-and-coverage", "Sanitizers, fuzzing and coverage"],
  ["core.testing.format-and-build", "08-formatting-cmake-and-ctest", "Formatting, CMake and ctest"],
  ["core.testing.reading-output", "09-debug-reading-test-output", "Reading test output"],
  ["core.testing.capstone-csv", "10-capstone-test-and-harden-a-library", "CSV capstone"],
];

await t.launch();

try {
  const page = await newPage();

  await page.goto(`${APP}/#/`);
  const module8 = page.locator("section.module", { hasText: "Testing and Tooling" });
  await module8.waitFor();
  check((await module8.locator(".lesson-list a").count()) === 1 + LESSONS.length, "Module 8 lists the test-out and ten lessons");

  // ---- what the learner sees when the tests are weak (the starter of the graded lessons) ----
  await open(page, "core.testing.googletest");
  await runTests(page, /tests passed|failed|did not compile/);
  const messages = (await page.locator(".results .tmsg").allTextContents()).join("\n");
  check(/Your tests still pass when/.test(messages), "a graded-tests lesson names the broken copies that the tests did not catch");

  // ---- the warning lesson builds with -Werror: the Problems tab explains the errors ----
  await open(page, "core.testing.warnings");
  await runTests(page, /did not compile/);
  await page.getByRole("tab", { name: /Problems/ }).click();
  await page.locator(".prob .ptext").first().waitFor();
  const explanations = (await page.locator(".prob .ptext").allTextContents()).join("\n");
  check(/same name/.test(explanations), "the shadowing warning is explained in plain language");
  check(/(fractional part|rounding|round)/.test(explanations), "the double-to-int conversion is explained");
  check(/forgets an enumerator|enumerator/.test(explanations), "the unhandled enumerator is explained");
  await shot(page, "m8-warnings-problems");

  // ---- every lesson, solved through the UI ----
  for (const [id, dir, label] of LESSONS) await solve(page, id, dir, label);
  await shot(page, "m8-capstone-pass");

  // ---------------- the module is complete by lessons alone ----------------
  await page.goto(`${APP}/#/`);
  const head = (await module8.locator("h2").textContent()) ?? "";
  check(/module complete/.test(head) && !/tested out/.test(head), `Module 8 is complete by finishing the lessons (${head.trim()})`);
  await shot(page, "m8-home");
  await page.close();

  // ---------------- the test-out completes it by itself (fresh storage) ----------------
  const fresh = await newPage();
  await open(fresh, "core.testing.test-out");
  let b = await runTests(fresh, /did not compile|tests passed|failed/);
  check(!/All tests passed/.test(b), `test-out: the initial tests do not pass (${b})`);
  await setFile(fresh, "stack_test.cpp", (await solutionFiles("00-test-out-test-the-stack"))["stack_test.cpp"]);
  b = await runTests(fresh, /All tests passed/);
  check(/All tests passed/.test(b), `test-out: ${b}`);
  await fresh.goto(`${APP}/#/`);
  const m8 = fresh.locator("section.module", { hasText: "Testing and Tooling" });
  check(((await m8.locator("h2 .badge.ok").textContent()) ?? "").includes("tested out"), "passing the test-out completes the module (tested out)");
  await shot(fresh, "m8-testout-home");
  await fresh.close();
} catch (e) {
  t.crashed(e);
}

await t.finish();
