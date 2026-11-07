// UI checks for Module 2 (Data and Memory): the Fast tier driven by allocation counting, and the rule that a
// module still being written is not completed by finishing the lessons that exist so far.
// Uses the real reference solutions from /content.
//   usage: node module2.mjs        env: APP_URL (default http://localhost:5173), SHOTS (screenshot dir)
import { readFile } from "node:fs/promises";
import { fileURLToPath } from "node:url";
import { chromium } from "playwright";

const APP = process.env.APP_URL ?? "http://localhost:5173";
const SHOTS = process.env.SHOTS;
const M2 = new URL("../../content/tracks/core/02-data-and-memory/", import.meta.url);
const solution = (lesson, file) => readFile(fileURLToPath(new URL(`${lesson}/solution/${file}`, M2)), "utf8");

let failures = 0;
const check = (ok, what) => {
  console.log(`${ok ? "PASS" : "FAIL"}  ${what}`);
  if (!ok) failures++;
};

const browser = await chromium.launch();
const ctx = await browser.newContext({ viewport: { width: 1400, height: 900 } }); // fresh storage
const page = await ctx.newPage();
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
const runTests = async (expected) => {
  await page.getByRole("tab", { name: "Tests" }).click();
  await page.getByRole("button", { name: /Run Tests/ }).click();
  await page.locator(".results .panel-body .banner", { hasText: expected }).first().waitFor({ timeout: 60000 });
  return ((await page.locator(".results .panel-body .banner").first().textContent()) ?? "").trim();
};

try {
  // ---------------- Arrays ----------------
  await open("core.data-and-memory.arrays");
  let b = await runTests(/did not compile|tests passed|failed/);
  check(!/All tests passed/.test(b), `arrays: the starter does not pass (${b})`);
  await setCode(await solution("01-arrays-and-std-array", "main.cpp"));
  b = await runTests(/All tests passed/);
  check(/All tests passed/.test(b), `arrays: ${b}`);

  // ---------------- vector: the Fast tier counts heap allocations ----------------
  await open("core.data-and-memory.vector");
  // a correct-but-wasteful solution (no reserve): Works and Robust pass, Fast fails on allocation counts
  const wasteful = (await solution("02-vector", "main.cpp")).replace("out.reserve(total);  // one allocation instead of repeated growth", "(void)total;");
  await setCode(wasteful);
  b = await runTests(/failed|tests passed/);
  check(!/All tests passed/.test(b), `vector: a solution without reserve does not pass everything (${b})`);
  const failText = (await page.locator(".results .panel-body").first().textContent()) ?? "";
  check(/heap allocation/.test(failText), "vector: the failure message talks about heap allocations");
  check(/fast/i.test((await page.locator(".results").textContent()) ?? ""), "vector: the Fast tier is shown");
  await shot("m2-vector-fast-fail");
  await setCode(await solution("02-vector", "main.cpp"));
  b = await runTests(/All tests passed/);
  check(/All tests passed/.test(b), `vector: the reference solution passes all tiers: ${b}`);
  await shot("m2-vector-pass");

  // ---------------- a module that is still being written ----------------
  await page.goto(`${APP}/#/`);
  const module2 = page.locator("section.module", { hasText: "Data and Memory" });
  await module2.waitFor();
  check(/More lessons are on the way/.test((await module2.textContent()) ?? ""), "Module 2 says more lessons are coming");
  check(await module2.locator("h2", { hasText: "2/2 complete" }).count() === 1, "both existing lessons count as complete");
  check((await module2.locator("h2 .badge.ok").count()) === 0, "finishing every lesson that exists does NOT mark the unfinished module complete");

  await page.goto(`${APP}/#/profile`);
  check((await page.locator(".badge-card.earned", { hasText: "Module Master" }).filter({ hasText: /2|Data/ }).count()) === 0, "no Module Master badge is awarded for the unfinished module");

  // ---------------- test-out still completes it ----------------
  await open("core.data-and-memory.test-out");
  b = await runTests(/did not compile|tests passed|failed/);
  check(!/All tests passed/.test(b), `test-out: the starter does not pass (${b})`);
  await setCode(await solution("00-test-out-rotate-and-dedupe", "main.cpp"));
  b = await runTests(/All tests passed/);
  check(/All tests passed/.test(b), `test-out: ${b}`);
  await page.goto(`${APP}/#/`);
  check((await page.locator("section.module", { hasText: "Data and Memory" }).locator("h2 .badge.ok").textContent())?.includes("module complete"), "passing the test-out completes the module");
  await shot("m2-home");
} catch (e) {
  failures++;
  console.log("FAIL  unexpected error:", e.message.split("\n").slice(0, 3).join(" | "));
  await shot("m2-error");
}

const real = consoleErrors.filter((e) => !/Failed to load resource|net::ERR/.test(e));
check(real.length === 0, `no unexpected browser console errors${real.length ? ": " + real[0].slice(0, 160) : ""}`);
await browser.close();
console.log(failures ? `\n${failures} check(s) failed` : "\nall checks passed");
process.exit(failures ? 1 : 0);
