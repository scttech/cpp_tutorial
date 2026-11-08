// UI check for sanitizer reports: the runner's REAL answer for a use-after-free and for a leak (from a build with
// -fsanitize) is played back into the app, and the results panel must explain it in plain language.
// (No lesson uses sanitizers yet, so the page cannot ask for them itself: this proves the display and the rules.)
//   usage: node sanitizers.mjs      env: APP_URL (default http://localhost:5173), RUNNER_URL (default http://localhost:8080), SHOTS
import { chromium } from "playwright";

const APP = process.env.APP_URL ?? "http://localhost:5173";
const RUNNER = process.env.RUNNER_URL ?? "http://localhost:8080";
const SHOTS = process.env.SHOTS;

let failures = 0;
const check = (ok, what) => {
  console.log(`${ok ? "PASS" : "FAIL"}  ${what}`);
  if (!ok) failures++;
};

const SAN = ["-std=c++20", "-fsanitize=address,undefined"];
async function realResult(source) {
  const res = await fetch(`${RUNNER}/run`, {
    method: "POST",
    headers: { "content-type": "application/json" },
    body: JSON.stringify({ mode: "run", userFiles: { "main.cpp": source }, flags: SAN, limits: { memoryMb: 256 } }),
  });
  if (!res.ok) throw new Error(`runner HTTP ${res.status}`);
  return res.json();
}

const uaf = await realResult("int main() {\n    int* p = new int(5);\n    delete p;\n    return *p;\n}\n");
const leak = await realResult("int main() {\n    int* p = new int[10];\n    p[0] = 1;\n    p = nullptr;\n}\n");
const overflow = await realResult("int main(int argc, char**) {\n    int big = 2147483647;\n    big += argc;\n    return big;\n}\n");
check(uaf.report?.kind === "heap-use-after-free" && leak.report?.kind === "leak" && overflow.report?.kind === "signed-integer-overflow", "the runner produced the three real reports");

const browser = await chromium.launch();
const consoleErrors = [];
try {
  const ctx = await browser.newContext({ viewport: { width: 1400, height: 900 } });
  const page = await ctx.newPage();
  page.on("pageerror", (e) => consoleErrors.push(String(e)));
  page.on("console", (m) => m.type() === "error" && consoleErrors.push(m.text()));
  let answer = uaf;
  const cors = { "access-control-allow-origin": "*", "access-control-allow-headers": "content-type", "access-control-allow-methods": "POST, GET, OPTIONS" };
  await page.route("**/run", (route) => {
    if (route.request().method() === "OPTIONS") return route.fulfill({ status: 204, headers: cors });
    return route.fulfill({ status: 200, headers: { ...cors, "content-type": "application/json" }, body: JSON.stringify(answer) });
  });

  async function run(result, headline) {
    answer = result;
    await page.getByRole("button", { name: /^▶ Run$/ }).click();
    // wait for THIS answer's card (the previous one is still on screen until the new result arrives)
    await page.locator(".runtime-card", { hasText: headline }).first().waitFor({ timeout: 15000 });
    return (await page.locator(".runtime-card").first().textContent()) ?? "";
  }

  await page.goto(`${APP}/#/lesson/core.foundations.hello-world`);
  await page.locator(".monaco-editor").first().waitFor({ timeout: 30000 });

  let text = await run(uaf, "Used memory after it was freed");
  check(/Used memory after it was freed/.test(text), "use-after-free: the headline says what happened");
  check(/main\.cpp:4/.test(text), "use-after-free: it points at main.cpp:4");
  check(/freed at main\.cpp:3/.test(text) && /allocated at main\.cpp:2/.test(text), "use-after-free: it says where the memory was freed and allocated");
  check(/After `?delete`?|pointer keeps its old address/.test(text) || /keeps its old address/.test(text), "use-after-free: it explains the cause in plain language");
  check((await page.locator(".runtime-card a.learn-more").count()) === 1, "use-after-free: a link to the lesson is offered");
  check((await page.locator(".runtime-card details.raw").count()) === 1, "use-after-free: the raw sanitizer report is one click away");
  await page.locator(".runtime-card details.raw summary").click();
  check(/AddressSanitizer: heap-use-after-free/.test((await page.locator(".runtime-card details.raw").textContent()) ?? ""), "the raw report is the real AddressSanitizer output");
  if (SHOTS) await page.screenshot({ path: `${SHOTS}/sanitizer-uaf.png` });

  text = await run(leak, "Memory was never freed");
  check(/Memory was never freed/.test(text) && /main\.cpp:2/.test(text), "leak: headline and the allocation site (main.cpp:2)");

  text = await run(overflow, "A signed integer overflowed");
  check(/A signed integer overflowed/.test(text) && /main\.cpp:3/.test(text), "undefined behavior: headline and location (main.cpp:3)");
  if (SHOTS) await page.screenshot({ path: `${SHOTS}/sanitizer-ub.png` });

  // a result without a report shows no card
  answer = { ...overflow, report: undefined, exitCode: 0, stderr: "" };
  await page.getByRole("button", { name: /^▶ Run$/ }).click();
  await page.waitForTimeout(800);
  check((await page.locator(".runtime-card").count()) === 0, "no report, no card");
  await ctx.close();
} catch (e) {
  failures++;
  console.log("FAIL  unexpected error:", e.message.split("\n").slice(0, 3).join(" | "));
}

const real = consoleErrors.filter((e) => !/Failed to load resource|net::ERR/.test(e));
check(real.length === 0, `no unexpected browser console errors${real.length ? ": " + real[0].slice(0, 160) : ""}`);
await browser.close();
console.log(failures ? `\n${failures} check(s) failed` : "\nall checks passed");
process.exit(failures ? 1 : 0);
