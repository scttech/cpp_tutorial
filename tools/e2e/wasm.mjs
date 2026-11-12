// UI checks of the STATIC build (VITE_RUNNER=wasm): the in-browser compiler runs lessons, and lessons it cannot run
// are greyed out. No Docker needed.
//   prerequisites: cd app && VITE_RUNNER=wasm npx vite build --outDir dist-wasm && npx vite preview --outDir dist-wasm --port 4173
//   usage: node wasm.mjs        env: APP_URL (default http://localhost:4173), SHOTS (screenshot dir)
import { readFile } from "node:fs/promises";
import { fileURLToPath } from "node:url";
import { createSuite } from "./lib.mjs";

const SLOW = 240000; // the first compile in a fresh browser loads ~90 MB of toolchain
const t = createSuite({ app: "http://localhost:4173", track: "", timeoutMs: SLOW });
const { APP, check, solutionFiles } = t;
// which lessons the compatibility report says cannot run in the browser (the course map must grey out exactly those)
const compat = JSON.parse(await readFile(fileURLToPath(new URL("../../content/wasm-compat.json", import.meta.url)), "utf8"));
const NOT_IN_BROWSER = Object.values(compat.lessons).filter((l) => !l.wasm).length;

await t.launch();
const page = await t.newPage();
const shot = (name) => t.shot(page, name);
const open = (id) => t.open(page, id);
const setCode = (text) => t.setCode(page, text);
const setFile = (name, text) => t.setFile(page, name, text);
const runTests = (expected) => t.runTests(page, expected);

try {
  // ---------------- the course map greys out what cannot run here ----------------
  await page.goto(`${APP}/#/`);
  await page.getByRole("link", { name: /^1. Hello, World/ }).waitFor();
  check(((await page.locator(".status, header").first().textContent()) ?? "").includes("In-browser compiler"), "the runner is named in the header");
  const locked = page.locator(".lesson-list li.locked");
  const names = (await locked.locator("span:first-child").allTextContents()).map((t) => t.trim());
  console.log("      greyed out:", names.join(" | "));
  check(names.length === NOT_IN_BROWSER, `exactly the ${NOT_IN_BROWSER} lessons the compatibility report marks are greyed out (${names.length})`);
  check((await locked.locator(".badge", { hasText: "Server needed" }).count()) === NOT_IN_BROWSER, "each shows a 'Server needed' badge");
  const tip = (await locked.first().getAttribute("title")) ?? "";
  check(/Not available in this version: .+Docker/.test(tip), `the tooltip says why (${tip.slice(0, 90)})`);
  check((await page.getByRole("link", { name: /^1. Hello, World/ }).count()) === 1, "compatible lessons stay clickable");
  await shot("w-home");

  // a greyed-out lesson opened by URL explains itself
  await page.goto(`${APP}/#/lesson/core.foundations.control-flow`);
  await page.getByText(/not available in this version/i).waitFor({ timeout: 15000 });
  check(true, "opening a greyed-out lesson by URL shows an explanation instead of an editor");

  // ---------------- run and test through the browser compiler ----------------
  await open("core.foundations.hello-world");
  const t0 = Date.now();
  let b = await runTests(/tests passed/);
  console.log(`      first run in a fresh browser: ${((Date.now() - t0) / 1000).toFixed(0)}s`);
  check(/1 of 2 tests passed/.test(b), `starter: partial pass (${b})`);
  check(((await page.locator(".tmsg").first().textContent()) ?? "").includes("expected"), "starter: the failure message is shown");
  check((await page.locator("[role=status]", { hasText: "Downloading the in-browser" }).count()) === 0, "the download banner is gone once the toolchain is loaded");
  await shot("w-starter");

  await setCode((await solutionFiles("01-foundations/01-hello-world"))["main.cpp"]);
  b = await runTests(/All tests passed/);
  check(/All tests passed/.test(b), `solution: ${b}`);

  await page.getByRole("button", { name: /^▶ Run$/ }).click();
  await page.getByRole("tab", { name: "Console" }).click();
  await page.locator(".console", { hasText: "Hello, World!" }).first().waitFor({ timeout: SLOW });
  check(true, "Run: the program's output appears in the console");

  // ---------------- errors read like the Docker version's ----------------
  await setCode('#include <iostream>\nint main() {\n    std::cout << "Hello, World!\\n"\n}\n');
  await page.getByRole("tab", { name: "Tests" }).click();
  await page.getByRole("button", { name: /Run Tests/ }).click();
  await page.getByText(/did not compile/).waitFor({ timeout: SLOW });
  check(((await page.locator(".start-here").textContent()) ?? "").length > 20, "a compile error comes with a friendly 'start here' explanation");
  await page.getByRole("tab", { name: /Problems/ }).click();
  check(/main\.cpp:3:/.test((await page.locator(".prob .loc").first().textContent()) ?? ""), "the problem points at main.cpp line 3");
  await shot("w-compile-error");

  await setCode("int f(int);\nint main() { return f(1); }\n");
  await page.getByRole("tab", { name: "Tests" }).click();
  await page.getByRole("button", { name: /Run Tests/ }).click();
  // (the previous compile error's banner is still showing, so wait for the linker text itself)
  await page.getByRole("tab", { name: /Problems/ }).click();
  await page.locator(".problems", { hasText: "find a definition" }).waitFor({ timeout: SLOW });
  const probs = (await page.locator(".problems").textContent()) ?? "";
  check(/find a definition for\s*f\(int\)/.test(probs) && /linker command failed/.test(probs), "a linker error gets the friendly 'no definition' explanation");

  // ---------------- cancel stops a runaway program, and the runner recovers ----------------
  await setCode("int main() { for (;;) {} }\n");
  await page.getByRole("button", { name: /^▶ Run$/ }).click();
  await page.getByRole("button", { name: "Cancel" }).waitFor({ timeout: SLOW });
  await page.waitForTimeout(500);
  await page.getByRole("button", { name: "Cancel" }).click();
  await page.getByText("Run cancelled").waitFor({ timeout: 20000 });
  check(true, "Cancel stops the run");
  await setCode((await solutionFiles("01-foundations/01-hello-world"))["main.cpp"]);
  b = await runTests(/All tests passed/);
  check(/All tests passed/.test(b), "after a cancel the next run works");

  const wall = Date.now();
  await setCode("#include <iostream>\nint main() { for (;;) {} }\n");
  await page.getByRole("button", { name: /^▶ Run$/ }).click();
  await page.getByRole("tab", { name: "Console" }).click();
  await page.locator(".badge.bad", { hasText: /timed out/ }).waitFor({ timeout: 60000 });
  check(Date.now() - wall < 30000, "an infinite loop is stopped by the time limit");

  // ---------------- real Module 2 lessons ----------------
  await open("core.data-and-memory.vector");
  await setCode((await solutionFiles("02-data-and-memory/02-vector"))["main.cpp"]);
  b = await runTests(/All tests passed/);
  check(/All tests passed/.test(b), `vector (allocation counting): ${b}`);
  await shot("w-vector");

  await open("core.data-and-memory.find-the-bug");
  const bug = await solutionFiles("02-data-and-memory/07-find-the-bug");
  await setFile("bugs.hpp", bug["bugs.hpp"]);
  await setFile("main.cpp", bug["main.cpp"]);
  b = await runTests(/All tests passed/);
  check(/All tests passed/.test(b), `find-the-bug (two files): ${b}`);

  // progress is recorded just like with the Docker runner
  await page.goto(`${APP}/#/`);
  const row = page.locator(".lesson-list li", { hasText: "std::vector" });
  check(((await row.textContent()) ?? "").includes("complete"), "completed lessons are marked on the course map");
} catch (e) {
  t.crashed(e);
  await shot("w-failure");
}

await t.finish();
