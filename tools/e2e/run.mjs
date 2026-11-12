// End-to-end check of the learner loop against a running dev server + runner.
//   prerequisites: `docker compose up -d runner`, and `npm run dev` in /app
//   usage: node run.mjs            env: APP_URL (default http://localhost:5173), SHOTS (screenshot dir)
import { createSuite } from "./lib.mjs";

const t = createSuite();
const { APP, check, shot } = t;
const LESSON = "core.foundations.hello-world";


await t.launch();
const page = await t.newPage();

const setCode = (text) => t.setCode(page, text);
// Click Run Tests, then wait for a result banner matching `expected` (avoids reading a stale result).
const runTests = async (expected) => {
  await page.getByRole("button", { name: /Run Tests/ }).click();
  await page.locator(".results .panel-body .banner", { hasText: expected }).first().waitFor({ timeout: 30000 });
};

try {
  // --- course map ---
  await page.goto(`${APP}/#/`);
  await page.getByRole("link", { name: /^1. Hello, World/ }).waitFor();
  check(true, "course map lists the Hello World lesson");
  await page.getByText(/Docker runner: online/).waitFor({ timeout: 15000 });
  check(true, "runner status shows online");
  await shot(page, "1-home");

  // --- starter fails ---
  await page.getByRole("link", { name: /^1. Hello, World/ }).click();
  await page.locator(".monaco-editor").first().waitFor({ timeout: 30000 });
  check((await page.locator(".md h1").textContent())?.includes("Hello, World"), "lesson text renders");
  await runTests(/tests passed/);
  const starterBanner = await page.locator(".results .banner").first().textContent();
  check(/1 of 2 tests passed/.test(starterBanner ?? ""), `starter: partial pass ("${starterBanner?.trim()}")`);
  check((await page.locator(".t-fail").count()) === 1, "starter: one failing test shown with an icon row");
  check(((await page.locator(".tmsg").first().textContent()) ?? "").includes("expected"), "starter: failure message shown");
  await shot(page, "2-starter-fails");

  // --- compile error -> problems + squiggle ---
  await setCode('#include <iostream>\nint main() {\n    std::cout << "Hello, World!\\n"\n}\n');
  await page.getByRole("button", { name: /Run Tests/ }).click();
  await page.getByText(/did not compile/).waitFor({ timeout: 30000 });
  check(true, "compile error reported");
  await page.getByRole("tab", { name: /Problems/ }).click();
  check((await page.locator(".prob.error").count()) >= 1, "problems tab lists the error");
  check((await page.locator(".squiggly-error").count()) >= 1, "editor shows an error squiggle");
  await shot(page, "3-compile-error");
  await page.locator(".prob.error").first().click(); // jump-to-line should not throw

  // --- fix it: all tests pass, lesson completes ---
  await setCode('#include <iostream>\nint main() {\n    std::cout << "Hello, World!\\n";\n    return 0;\n}\n');
  await page.getByRole("tab", { name: "Tests" }).click();
  await runTests(/All tests passed/);
  const passBanner = await page.locator(".results .banner").first().textContent();
  check(/All tests passed/.test(passBanner ?? ""), `solution: ${passBanner?.trim()}`);
  check((await page.getByText("earned").count()) >= 1, "Works tier marked earned");
  await shot(page, "4-pass");

  // --- plain Run + stdin console ---
  await page.getByRole("button", { name: /^▶ Run$/ }).click();
  await page.getByRole("tab", { name: "Console" }).click();
  // wait for the real Run result: the previous (test) run has empty stdout, so poll for the text
  await page.locator(".console", { hasText: "Hello, World!" }).first().waitFor({ timeout: 30000 });
  const consoleText = (await page.locator(".console").first().textContent()) ?? "";
  check(consoleText.includes("Hello, World!"), `Run shows program output in the console (got ${JSON.stringify(consoleText)})`);
  await shot(page, "4b-console");

  // --- progress persists + shows on the map ---
  await page.getByRole("link", { name: /Course map/ }).click();
  await page.getByText("✔ complete").waitFor();
  check(true, "course map shows the lesson complete");
  await page.reload();
  await page.getByText("✔ complete").waitFor();
  check(true, "progress survives a reload");
  await shot(page, "5-home-complete");

  // --- hints ladder counts reveals ---
  await page.getByRole("link", { name: /^1. Hello, World/ }).click();
  await page.getByRole("tab", { name: "Hints" }).click();
  await page.getByRole("button", { name: /Reveal level 1/ }).click();
  check((await page.getByText(/Run the tests and read the failure message/).count()) === 1, "hint level 1 reveals on demand");
  await shot(page, "6-hints");

  // --- runner down: friendly error, no crash ---
  await page.route("**/localhost:8080/run", (r) => r.abort());
  await page.getByRole("tab", { name: "Lesson" }).click();
  await page.getByRole("button", { name: /Run Tests/ }).click();
  await page.getByText(/Cannot reach the runner/).waitFor({ timeout: 10000 });
  check(true, "unreachable runner shows a friendly message");
} catch (e) {
  t.crashed(e);
  await shot(page, "error");
}

await t.finish();
