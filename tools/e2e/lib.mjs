// Shared plumbing for the browser (Playwright) checks, so each script only says WHAT it checks.
//
//   const t = createSuite({ app: "http://localhost:5173", track: "03-classes-and-objects/" });
//   await t.launch();
//   try {
//     const page = await t.newPage();               // a fresh browser context: empty storage, console errors collected
//     await t.solve(page, "core.classes.x", "01-x", "label");
//     t.check(cond, "what is true");
//   } catch (e) { t.crashed(e); }
//   await t.finish();                               // checks for console errors, closes the browser, sets the exit code
//
// env: APP_URL overrides the app address, SHOTS is a directory for screenshots.
import { readFile, readdir } from "node:fs/promises";
import { fileURLToPath } from "node:url";
import { chromium } from "playwright";

export function createSuite({ app = "http://localhost:5173", track, timeoutMs = 90000 } = {}) {
  const APP = process.env.APP_URL ?? app;
  const SHOTS = process.env.SHOTS;
  const trackBase = track !== undefined ? new URL(`../../content/tracks/${track.startsWith("domains/") ? "" : "core/"}${track}`, import.meta.url) : null;
  const consoleErrors = [];
  let failures = 0;
  let browser;

  const check = (ok, what) => {
    console.log(`${ok ? "PASS" : "FAIL"}  ${what}`);
    if (!ok) failures++;
  };

  const launch = async () => (browser = await chromium.launch());

  /** Collects console errors and accepts confirm() dialogs. */
  const watch = (page) => {
    page.on("pageerror", (e) => consoleErrors.push(String(e)));
    page.on("console", (m) => m.type() === "error" && consoleErrors.push(m.text()));
    page.on("dialog", (d) => d.accept());
    return page;
  };

  /** A page in a brand-new browser context (empty storage). `init` is a script to run before the app loads. */
  async function newPage({ init, acceptDownloads = false } = {}) {
    const ctx = await browser.newContext({ viewport: { width: 1400, height: 900 }, acceptDownloads });
    if (init) await ctx.addInitScript(init);
    return watch(await ctx.newPage());
  }

  const shot = async (page, name) => SHOTS && page.screenshot({ path: `${SHOTS}/${name}.png` });

  async function open(page, lessonId) {
    await page.goto(`${APP}/#/lesson/${lessonId}`);
    await page.locator(".monaco-editor").first().waitFor({ timeout: 30000 });
  }

  /** Replace the contents of the visible editor (insertText bypasses auto-closing brackets). */
  async function setCode(page, text) {
    await page.locator(".monaco-editor .view-lines").first().click();
    await page.keyboard.press("Control+A");
    // insertText is typing to Monaco, which auto-indents every new line ON TOP of the indentation in the text; that compounds on long files and
    // can exceed the runner's size limit. The e2e does not care about indentation, so the leading blanks are dropped and Monaco re-indents.
    await page.keyboard.insertText(text.replace(/^[ 	]+/gm, ""));
  }

  /** Switch to the file's tab and replace its contents. */
  async function setFile(page, name, text) {
    await page.getByRole("tab", { name: new RegExp(`^${name.replace(".", "\\.")}`) }).click();
    await page.waitForTimeout(120);
    await setCode(page, text);
  }

  /** Click Run Tests and wait for a banner matching `expected` (so a stale result is never read). Returns the banner text. */
  async function runTests(page, expected, timeout = timeoutMs) {
    await page.getByRole("tab", { name: "Tests" }).click();
    await page.getByRole("button", { name: /Run Tests/ }).click();
    await page.locator(".results .panel-body .banner", { hasText: expected }).first().waitFor({ timeout });
    return ((await page.locator(".results .panel-body .banner").first().textContent()) ?? "").trim();
  }

  /** The files of a lesson's reference solution, as { name: text } (every file in solution/ is an editable file). */
  async function solutionFiles(dir) {
    const base = new URL(`${dir}/solution/`, trackBase);
    const out = {};
    for (const f of await readdir(fileURLToPath(base))) out[f] = await readFile(fileURLToPath(new URL(f, base)), "utf8");
    return out;
  }

  /** Open a lesson, check that its starter does not pass, put in the reference solution, check that it passes. */
  async function solve(page, id, dir, label) {
    await open(page, id);
    const sol = await solutionFiles(dir);
    const first = await runTests(page, /did not compile|tests passed|failed|time limit/);
    check(!/All tests passed/.test(first), `${label}: the initial code does not pass (${first})`);
    for (const [name, text] of Object.entries(sol)) await setFile(page, name, text);
    const b = await runTests(page, /All tests passed/);
    check(/All tests passed/.test(b), `${label}: ${b}`);
  }

  function crashed(e, lines = 3) {
    failures++;
    console.log("FAIL  unexpected error:", e.message.split("\n").slice(0, lines).join(" | "));
  }

  async function finish() {
    const real = consoleErrors.filter((e) => !/Failed to load resource|net::ERR/.test(e));
    check(real.length === 0, `no unexpected browser console errors${real.length ? ": " + real[0].slice(0, 200) : ""}`);
    await browser.close();
    console.log(failures ? `\n${failures} check(s) failed` : "\nall checks passed");
    process.exit(failures ? 1 : 0);
  }

  return { APP, SHOTS, check, launch, watch, newPage, shot, open, setCode, setFile, runTests, solutionFiles, solve, crashed, finish, get browser() { return browser; } };
}
