// UI checks for Module 2 (Data and Memory): every lesson is solved through the real UI with its reference
// solution (including the multi-file ones), the Fast tier is driven by allocation counting, the module completes
// and awards its badges, and the test-out still completes the module on its own.
//   usage: node module2.mjs        env: APP_URL (default http://localhost:5173), SHOTS (screenshot dir)
import { readFile, readdir } from "node:fs/promises";
import { fileURLToPath } from "node:url";
import { chromium } from "playwright";

const APP = process.env.APP_URL ?? "http://localhost:5173";
const SHOTS = process.env.SHOTS;
const M2 = new URL("../../content/tracks/core/02-data-and-memory/", import.meta.url);
const solutionFiles = async (dir) => {
  const base = new URL(`${dir}/solution/`, M2);
  const out = {};
  for (const f of await readdir(fileURLToPath(base))) out[f] = await readFile(fileURLToPath(new URL(f, base)), "utf8");
  return out;
};

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
/** Solve a lesson with its reference solution; `files` lists the editable files (tab order). */
async function solve(page, id, dir, label) {
  await open(page, id);
  const sol = await solutionFiles(dir);
  const first = await runTests(page, /did not compile|tests passed|failed/);
  check(!/All tests passed/.test(first), `${label}: the initial code does not pass (${first})`);
  for (const [name, text] of Object.entries(sol)) await setFile(page, name, text);
  const b = await runTests(page, /All tests passed/);
  check(/All tests passed/.test(b), `${label}: ${b}`);
}

try {
  const page = await newPage();

  await solve(page, "core.data-and-memory.arrays", "01-arrays-and-std-array", "arrays");

  // ---------------- vector: the Fast tier counts heap allocations ----------------
  await open(page, "core.data-and-memory.vector");
  const wasteful = (await solutionFiles("02-vector"))["main.cpp"].replace("out.reserve(total);  // one allocation instead of repeated growth", "(void)total;");
  await setCode(page, wasteful);
  let b = await runTests(page, /failed|tests passed/);
  check(!/All tests passed/.test(b), `vector: a solution without reserve does not pass everything (${b})`);
  check(/heap allocation/.test((await page.locator(".results .panel-body").first().textContent()) ?? ""), "vector: the failure message talks about heap allocations");
  check(/fast/i.test((await page.locator(".results").textContent()) ?? ""), "vector: the Fast tier is shown");
  await shot(page, "m2-vector-fast-fail");
  await setCode(page, (await solutionFiles("02-vector"))["main.cpp"]);
  b = await runTests(page, /All tests passed/);
  check(/All tests passed/.test(b), `vector: the reference solution passes all tiers: ${b}`);

  await solve(page, "core.data-and-memory.strings", "03-strings-and-string-view", "strings");
  await solve(page, "core.data-and-memory.references", "04-references", "references");

  // ---------------- pointers: leak / double-free detection through the UI ----------------
  await open(page, "core.data-and-memory.pointers");
  const leaky = (await solutionFiles("05-pointers-and-the-heap"))["main.cpp"].replace("    delete cur;                                                 // ...then delete\n", "");
  await setCode(page, leaky);
  b = await runTests(page, /failed|tests passed/);
  check(!/All tests passed/.test(b), `pointers: a remove_first that forgets to delete does not pass (${b})`);
  check(/memory leak/.test((await page.locator(".results .panel-body").first().textContent()) ?? ""), "pointers: the Clean tier reports the leak");
  await shot(page, "m2-pointers-leak");
  await solve(page, "core.data-and-memory.pointers", "05-pointers-and-the-heap", "pointers");

  await solve(page, "core.data-and-memory.structs", "06-structs-and-classes", "structs (geometry.hpp)");

  // ---------------- find the bug: two files ----------------
  await open(page, "core.data-and-memory.find-the-bug");
  b = await runTests(page, /failed|did not compile|tests passed/);
  const bugText = (await page.locator(".results .panel-body").first().textContent()) ?? "";
  check(/-572662307/.test(bugText), "find-the-bug: the use-after-free shows the 0xDDDDDDDD pattern");
  await shot(page, "m2-find-the-bug");
  const bugSol = await solutionFiles("07-find-the-bug");
  await setFile(page, "bugs.hpp", bugSol["bugs.hpp"]);
  await setFile(page, "main.cpp", bugSol["main.cpp"]);
  b = await runTests(page, /All tests passed/);
  check(/All tests passed/.test(b), `find-the-bug: ${b}`);

  // ---------------- capstone ----------------
  await open(page, "core.data-and-memory.capstone-inventory");
  await setCode(page, (await solutionFiles("08-capstone-inventory-tracker"))["main.cpp"]);
  await page.locator("details.stdin summary").click();
  await page.locator("details.stdin textarea").fill("add widget 5 2.50\nadd bolt 10 0.25\nreport\nremove widget 9\nquit\n");
  await page.getByRole("button", { name: /^▶ Run$/ }).click();
  await page.getByRole("tab", { name: "Console" }).click();
  await page.locator(".console", { hasText: "total:" }).first().waitFor({ timeout: 30000 });
  const out = await page.locator(".console").first().textContent();
  check(out === "added 5 widget (now 5)\nadded 10 bolt (now 10)\nbolt x10 @ 0.25 = 2.50\nwidget x5 @ 2.50 = 12.50\ntotal: 15.00\nerror: not enough widget (have 5)\n", "capstone: Run feeds stdin and prints the report");
  b = await runTests(page, /All tests passed/);
  check(/All tests passed/.test(b), `capstone: ${b}`);
  await shot(page, "m2-capstone-pass");

  // ---------------- the module is complete by lessons alone ----------------
  await page.goto(`${APP}/#/`);
  const module2 = page.locator("section.module", { hasText: "Data and Memory" });
  await module2.waitFor();
  const head = (await module2.locator("h2").textContent()) ?? "";
  check(/module complete/.test(head) && !/tested out/.test(head), `Module 2 is complete by finishing the lessons (${head.trim()})`);
  check((await module2.locator(".lesson-list a").count()) === 9, "Module 2 lists test-out + 7 lessons + capstone");
  check(!/More lessons are on the way/.test((await module2.textContent()) ?? ""), "the 'more lessons are on the way' note is gone");
  await shot(page, "m2-home");

  await page.goto(`${APP}/#/profile`);
  await page.locator(".badge-card.earned").first().waitFor();
  const earned = (await page.locator(".badge-card.earned .bname").allTextContents()).map((t) => t.trim());
  for (const name of ["Module Master II", "Pointer Whisperer", "Bug Hunter", "Need for Speed", "Squeaky Clean", "Flawless"]) {
    check(earned.some((n) => n.includes(name)), `badge earned: ${name}`);
  }
  await shot(page, "m2-profile");
  await page.close();

  // ---------------- the test-out completes the module by itself (fresh storage) ----------------
  const fresh = await newPage();
  await open(fresh, "core.data-and-memory.test-out");
  b = await runTests(fresh, /did not compile|tests passed|failed/);
  check(!/All tests passed/.test(b), `test-out: the initial code does not pass (${b})`);
  await setCode(fresh, (await solutionFiles("00-test-out-rotate-and-dedupe"))["main.cpp"]);
  b = await runTests(fresh, /All tests passed/);
  check(/All tests passed/.test(b), `test-out: ${b}`);
  await fresh.goto(`${APP}/#/`);
  check(((await fresh.locator("section.module", { hasText: "Data and Memory" }).locator("h2 .badge.ok").textContent()) ?? "").includes("tested out"), "passing the test-out completes the module (tested out)");
  await shot(fresh, "m2-testout-home");
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
