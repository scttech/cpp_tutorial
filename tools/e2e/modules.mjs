// UI checks for the features introduced by Module 1 lessons 6 and 7: the multi-file editor with
// read-only tabs, and stdin-driven Run. Uses the real reference solutions from /content.
//   usage: node modules.mjs        env: APP_URL (default http://localhost:5173), SHOTS (screenshot dir)
import { readFile } from "node:fs/promises";
import { fileURLToPath } from "node:url";
import { chromium } from "playwright";

const APP = process.env.APP_URL ?? "http://localhost:5173";
const SHOTS = process.env.SHOTS;
const M1 = new URL("../../content/tracks/core/01-foundations/", import.meta.url);
const solution = (lesson, file) => readFile(fileURLToPath(new URL(`${lesson}/solution/${file}`, M1)), "utf8");

let failures = 0;
const check = (ok, what) => {
  console.log(`${ok ? "PASS" : "FAIL"}  ${what}`);
  if (!ok) failures++;
};

const browser = await chromium.launch();
const page = await browser.newPage({ viewport: { width: 1400, height: 900 } });
const consoleErrors = [];
page.on("pageerror", (e) => consoleErrors.push(String(e)));
page.on("console", (m) => m.type() === "error" && consoleErrors.push(m.text()));
page.on("dialog", (d) => d.accept());
const shot = async (name) => SHOTS && page.screenshot({ path: `${SHOTS}/${name}.png` });

async function open(lessonId) {
  await page.goto(`${APP}/#/lesson/${lessonId}`);
  await page.locator(".monaco-editor").first().waitFor({ timeout: 30000 });
}
async function setCode(text) {
  await page.locator(".monaco-editor .view-lines").first().click();
  await page.keyboard.press("Control+A");
  await page.keyboard.insertText(text);
}
const bannerAfterClick = async (button, expected) => {
  await page.getByRole("button", { name: button }).click();
  await page.locator(".results .panel-body .banner", { hasText: expected }).first().waitFor({ timeout: 40000 });
  return (await page.locator(".results .panel-body .banner").first().textContent())?.trim();
};

try {
  // ---------------- Lesson 6: multi-file ----------------
  await open("core.foundations.headers-and-build-model");
  const tabs = await page.locator('.editor [role="tab"]').allTextContents();
  check(tabs.length === 4, `four file tabs shown (${tabs.map((t) => t.trim()).join(" | ")})`);
  check(
    tabs.map((t) => t.replace(/\s*\(read-only\)/, "").trim()).join(",") === "circle.hpp,circle.cpp,main.cpp,shapes.hpp",
    "tabs follow lesson.yaml order: editable files first, then read-only",
  );
  check(tabs.filter((t) => t.includes("read-only")).length === 2, "main.cpp and shapes.hpp are marked read-only");
  check((await page.locator(".editor .tab.on").textContent())?.includes("circle.hpp"), "the first editable file is selected");

  // read-only tab cannot be typed into
  await page.getByRole("tab", { name: /^main\.cpp/ }).click();
  const before = await page.locator(".monaco-editor .view-lines").first().textContent();
  await page.locator(".monaco-editor .view-lines").first().click();
  await page.keyboard.type("XYZ_SHOULD_NOT_APPEAR");
  const after = await page.locator(".monaco-editor .view-lines").first().textContent();
  check(before === after && !after.includes("XYZ_SHOULD_NOT_APPEAR"), "read-only file ignores typing");

  // REGRESSION: switching tabs must never copy one file's text into another (a stale-closure bug
  // once overwrote the previously active file with the newly selected file's content).
  // Monaco renders spaces as non-breaking spaces in the DOM
  const viewText = async () => ((await page.locator(".monaco-editor .view-lines").first().textContent()) ?? "").replace(/ /g, " ");
  for (const name of ["circle.hpp", "circle.cpp", "shapes.hpp", "main.cpp", "circle.hpp"]) {
    await page.getByRole("tab", { name: new RegExp(`^${name.replace(".", "\\.")}`) }).click();
    await page.waitForTimeout(150);
  }
  const hppText = await viewText();
  check(hppText.includes("TODO: this header is missing everything") && !hppText.includes("int main"), "tab switching leaves each file's content intact");
  await page.getByRole("tab", { name: /^main\.cpp/ }).click();

  // starter: build fails with a diagnostic
  let b = await bannerAfterClick(/Run Tests/, /did not compile/);
  check(/did not compile/.test(b ?? ""), "starter: build failure is reported");
  await page.getByRole("tab", { name: /Problems/ }).click();
  check((await page.locator(".prob.error").count()) >= 1, "starter: problems listed");
  const problemText = (await page.locator(".problems").textContent()) ?? "";
  check(!/nested too deeply|redefinition of 'main'/.test(problemText), "starter: errors are about the missing declarations, not a corrupted file");
  await shot("m1-lesson6-starter");

  // solve across two files
  await page.getByRole("tab", { name: /^circle\.hpp/ }).click();
  await setCode(await solution("06-headers-and-build-model", "circle.hpp"));
  await page.getByRole("tab", { name: /^circle\.cpp/ }).click();
  await setCode(await solution("06-headers-and-build-model", "circle.cpp"));
  await page.getByRole("tab", { name: "Tests" }).click();
  b = await bannerAfterClick(/Run Tests/, /All tests passed|tests passed/);
  check(/All tests passed/.test(b ?? ""), `lesson 6 solved across two files: ${b}`);
  check((await page.getByText("earned").count()) >= 2, "Works and Robust tiers both earned");
  await shot("m1-lesson6-pass");

  // ---------------- Lesson 7: stdin ----------------
  await open("core.foundations.io-basics");
  await setCode(await solution("07-io-basics", "main.cpp"));
  await page.locator("details.stdin summary").click();
  await page.locator("details.stdin textarea").fill("Ada Lovelace 36\nBob\n\nCarol 30x\n");
  await page.getByRole("button", { name: /^▶ Run$/ }).click();
  await page.getByRole("tab", { name: "Console" }).click();
  await page.locator(".console", { hasText: "Valid: 1, Invalid: 2" }).first().waitFor({ timeout: 30000 });
  const out = await page.locator(".console").first().textContent();
  check(out === "Ada Lovelace is 36 years old\nInvalid line: Bob\nInvalid line: Carol 30x\nValid: 1, Invalid: 2\n", "Run feeds the stdin box to the program");
  await shot("m1-lesson7-run");
  await page.getByRole("tab", { name: "Tests" }).click();
  b = await bannerAfterClick(/Run Tests/, /tests passed/);
  check(/All tests passed/.test(b ?? ""), `lesson 7: ${b}`);

  // ---------------- Capstone: Unit Converter CLI ----------------
  await open("core.foundations.capstone-unit-converter");
  await setCode(await solution("08-capstone-unit-converter", "main.cpp"));
  await page.locator("details.stdin summary").click();
  await page.locator("details.stdin textarea").fill("convert 100 C F\nconvert 5 km kg\nquit\n");
  await page.getByRole("button", { name: /^▶ Run$/ }).click();
  await page.getByRole("tab", { name: "Console" }).click();
  await page.locator(".console", { hasText: "below" }).or(page.locator(".console", { hasText: "cannot convert" })).first().waitFor({ timeout: 30000 });
  const capOut = await page.locator(".console").first().textContent();
  check(capOut === "100 C = 212.0000 F\nerror: cannot convert km to kg\n", "capstone: Run converts and reports an error from stdin");
  await page.getByRole("tab", { name: "Tests" }).click();
  b = await bannerAfterClick(/Run Tests/, /tests passed/);
  check(/All tests passed/.test(b ?? ""), `capstone: ${b}`);

  // ---------------- Test-out: Temperature table ----------------
  await open("core.foundations.test-out");
  b = await bannerAfterClick(/Run Tests/, /did not compile|tests passed/);
  check(!/All tests passed/.test(b ?? ""), `test-out: the empty starter does not pass (${b})`);
  await setCode(await solution("00-test-out-temperature-table", "main.cpp"));
  await page.getByRole("tab", { name: "Tests" }).click();
  b = await bannerAfterClick(/Run Tests/, /All tests passed/);
  check(/All tests passed/.test(b ?? ""), `test-out: ${b}`);
  await shot("m1-testout-pass");

  // ---------------- course map ----------------
  await page.goto(`${APP}/#/`);
  await page.getByRole("link", { name: /I\/O basics/ }).waitFor();
  const links = await page.locator(".lesson-list a").count();
  check(links === 9, `course map lists test-out + 7 lessons + capstone (${links})`);
  check((await page.locator(".lesson-list li").first().getAttribute("class"))?.includes("test-out"), "the test-out is listed first");
  check((await page.locator("h2 .badge.ok").first().textContent())?.includes("tested out"), "passing the test-out marks the module complete (tested out)");
  await shot("m1-home");
} catch (e) {
  failures++;
  console.log("FAIL  unexpected error:", e.message.split("\n")[0]);
  await shot("m1-error");
}

const real = consoleErrors.filter((e) => !/Failed to load resource|net::ERR/.test(e));
check(real.length === 0, `no unexpected browser console errors${real.length ? ": " + real[0].slice(0, 160) : ""}`);
await browser.close();
console.log(failures ? `\n${failures} check(s) failed` : "\nall checks passed");
process.exit(failures ? 1 : 0);
