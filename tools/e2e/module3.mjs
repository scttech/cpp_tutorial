// UI checks for Module 3 (Classes and Objects): every lesson is solved through the real UI with its reference
// solution, finishing them all completes the module, and the test-out completes it on its own.
//   usage: node module3.mjs        env: APP_URL (default http://localhost:5173), SHOTS (screenshot dir)
import { readFile, readdir } from "node:fs/promises";
import { fileURLToPath } from "node:url";
import { chromium } from "playwright";

const APP = process.env.APP_URL ?? "http://localhost:5173";
const SHOTS = process.env.SHOTS;
const M3 = new URL("../../content/tracks/core/03-classes-and-objects/", import.meta.url);
const solutionFiles = async (dir) => {
  const base = new URL(`${dir}/solution/`, M3);
  const out = {};
  for (const f of await readdir(fileURLToPath(base))) out[f] = await readFile(fileURLToPath(new URL(f, base)), "utf8");
  return out;
};
const LESSONS = [
  ["core.classes.constructors", "01-constructors-and-explicit", "Constructors"],
  ["core.classes.encapsulation", "02-encapsulation-and-invariants", "Encapsulation"],
  ["core.classes.const-correctness", "03-const-correctness", "Const correctness"],
  ["core.classes.operators", "04-operator-overloading", "Operators"],
  ["core.classes.static-and-enums", "05-static-members-and-enums", "Static and enums"],
  ["core.classes.namespaces", "06-namespaces-and-modular-design", "Namespaces"],
  ["core.classes.capstone-library", "07-capstone-library", "Capstone"],
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
  const module3 = page.locator("section.module", { hasText: "Classes and Objects" });
  await module3.waitFor();
  check((await module3.locator(".lesson-list a").count()) === 1 + LESSONS.length, "Module 3 lists the test-out, six lessons and the capstone");
  check(!/More lessons are on the way/.test((await module3.textContent()) ?? ""), "the 'more lessons are on the way' note is gone");

  // ---------------- the first lesson shows the Docker runner's friendly failure for a classic mistake ----------------
  await open(page, "core.classes.constructors");
  const mistake = (await solutionFiles("01-constructors-and-explicit"))["money.hpp"].replace("    explicit Money(long long cents)", "    Money(long long cents)");
  await setFile(page, "money.hpp", mistake);
  const b0 = await runTests(page, /failed|did not compile|tests passed/);
  check(!/All tests passed/.test(b0), `constructors: dropping 'explicit' does not pass everything (${b0})`);
  check(/explicit/.test((await page.locator(".results .panel-body").first().textContent()) ?? ""), "constructors: the failure message talks about explicit");
  await shot(page, "m3-constructors-fail");

  for (const [id, dir, label] of LESSONS) await solve(page, id, dir, label);
  await shot(page, "m3-capstone-pass");

  // ---------------- the module is complete by lessons alone ----------------
  await page.goto(`${APP}/#/`);
  const head = (await module3.locator("h2").textContent()) ?? "";
  check(/module complete/.test(head) && !/tested out/.test(head), `Module 3 is complete by finishing the lessons (${head.trim()})`);
  await shot(page, "m3-home");
  await page.close();

  // ---------------- the test-out completes it by itself (fresh storage) ----------------
  const fresh = await newPage();
  await open(fresh, "core.classes.test-out");
  let b = await runTests(fresh, /did not compile|tests passed|failed/);
  check(!/All tests passed/.test(b), `test-out: the initial code does not pass (${b})`);
  await setFile(fresh, "fraction.hpp", (await solutionFiles("00-test-out-fraction"))["fraction.hpp"]);
  b = await runTests(fresh, /All tests passed/);
  check(/All tests passed/.test(b), `test-out: ${b}`);
  await fresh.goto(`${APP}/#/`);
  const m3 = fresh.locator("section.module", { hasText: "Classes and Objects" });
  check(((await m3.locator("h2 .badge.ok").textContent()) ?? "").includes("tested out"), "passing the test-out completes the module (tested out)");
  await shot(fresh, "m3-testout-home");
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
