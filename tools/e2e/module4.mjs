// UI checks for Module 4 (Ownership and RAII): every lesson is solved through the real UI with its reference
// solution, finishing them all completes the module, and the test-out completes it on its own.
//   usage: node module4.mjs        env: APP_URL (default http://localhost:5173), SHOTS (screenshot dir)
import { readFile, readdir } from "node:fs/promises";
import { fileURLToPath } from "node:url";
import { chromium } from "playwright";

const APP = process.env.APP_URL ?? "http://localhost:5173";
const SHOTS = process.env.SHOTS;
const M4 = new URL("../../content/tracks/core/04-ownership-and-raii/", import.meta.url);
const solutionFiles = async (dir) => {
  const base = new URL(`${dir}/solution/`, M4);
  const out = {};
  for (const f of await readdir(fileURLToPath(base))) out[f] = await readFile(fileURLToPath(new URL(f, base)), "utf8");
  return out;
};
const LESSONS = [
  ["core.ownership.raii", "01-destructors-and-raii", "Destructors and RAII"],
  ["core.ownership.rule-of-three", "02-copying-and-the-rule-of-three", "Rule of three"],
  ["core.ownership.rule-of-five", "03-moving-and-the-rule-of-five", "Rule of five"],
  ["core.ownership.unique-ptr", "04-unique-ptr-and-the-rule-of-zero", "unique_ptr"],
  ["core.ownership.shared-ptr", "05-shared-ptr-and-weak-ptr", "shared_ptr"],
  ["core.ownership.find-the-bug", "06-find-the-lifetime-bug", "Lifetime bugs"],
  ["core.ownership.capstone-vector", "07-capstone-int-vector", "Capstone"],
];

let failures = 0;
const check = (ok, what) => {
  console.log(`${ok ? "PASS" : "FAIL"}  ${what}`);
  if (!ok) failures++;
};

const browser = await chromium.launch();
const consoleErrors = [];
async function newPage() {
  const ctx = await browser.newContext({ viewport: { width: 1400, height: 900 } }); // fresh storage
  const page = await ctx.newPage();
  page.on("pageerror", (e) => consoleErrors.push(String(e)));
  page.on("console", (m) => m.type() === "error" && consoleErrors.push(m.text()));
  page.on("dialog", (d) => d.accept());
  return page;
}
const shot = async (page, name) => SHOTS && page.screenshot({ path: `${SHOTS}/${name}.png` });

async function open(page, lessonId) {
  await page.goto(`${APP}/#/lesson/${lessonId}`);
  await page.locator(".monaco-editor").first().waitFor({ timeout: 30000 });
}
async function setCode(page, text) {
  await page.locator(".monaco-editor .view-lines").first().click();
  await page.keyboard.press("Control+A");
  await page.keyboard.insertText(text);
}
async function setFile(page, name, text) {
  await page.getByRole("tab", { name: new RegExp(`^${name.replace(".", "\\.")}`) }).click();
  await page.waitForTimeout(120);
  await setCode(page, text);
}
const runTests = async (page, expected) => {
  await page.getByRole("tab", { name: "Tests" }).click();
  await page.getByRole("button", { name: /Run Tests/ }).click();
  await page.locator(".results .panel-body .banner", { hasText: expected }).first().waitFor({ timeout: 90000 });
  return ((await page.locator(".results .panel-body .banner").first().textContent()) ?? "").trim();
};
/** Solve a lesson with its reference solution (every file in solution/ is an editable file). */
async function solve(page, id, dir, label) {
  await open(page, id);
  const first = await runTests(page, /did not compile|tests passed|failed/);
  check(!/All tests passed/.test(first), `${label}: the initial code does not pass (${first})`);
  for (const [name, text] of Object.entries(await solutionFiles(dir))) await setFile(page, name, text);
  const b = await runTests(page, /All tests passed/);
  check(/All tests passed/.test(b), `${label}: ${b}`);
}

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
  failures++;
  console.log("FAIL  unexpected error:", e.message.split("\n").slice(0, 3).join(" | "));
}

const real = consoleErrors.filter((e) => !/Failed to load resource|net::ERR/.test(e));
check(real.length === 0, `no unexpected browser console errors${real.length ? ": " + real[0].slice(0, 160) : ""}`);
await browser.close();
console.log(failures ? `\n${failures} check(s) failed` : "\nall checks passed");
process.exit(failures ? 1 : 0);
