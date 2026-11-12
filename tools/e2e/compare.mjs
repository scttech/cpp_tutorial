// End-to-end checks for "Compare with reference": offered only after a lesson is passed, counts as revealing the solution, shows a diff
// per file, and hands your solution to the tutor for review. Needs the Docker runner (the lesson is really solved first).
//   usage: node compare.mjs        env: APP_URL (default http://localhost:5173), SHOTS (screenshot dir)
import { createSuite } from "./lib.mjs";

const t = createSuite({ track: "02-data-and-memory/" });
const { check, shot } = t;
const LESSON = "core.data-and-memory.arrays";
const DIR = "01-arrays-and-std-array";
const KEY = "sk-ant-api03-mock-key-for-tests-0000000000000000";

const sse = (text) => {
  const ev = (type, data) => `event: ${type}\ndata: ${JSON.stringify({ type, ...data })}\n\n`;
  return ev("message_start", { message: { usage: { input_tokens: 10, output_tokens: 1 } } }) + ev("content_block_delta", { delta: { type: "text_delta", text } }) + ev("message_delta", { delta: { stop_reason: "end_turn" }, usage: { output_tokens: 5 } });
};
const CORS = { "access-control-allow-origin": "*", "access-control-allow-headers": "*", "access-control-allow-methods": "POST, OPTIONS" };

const idb = (page) =>
  page.evaluate(
    () =>
      new Promise((resolve) => {
        const open = indexedDB.open("zth", 1);
        open.onerror = () => resolve(null);
        open.onsuccess = () => {
          const get = open.result.transaction("kv").objectStore("kv").get("progress");
          get.onsuccess = () => resolve(get.result ?? null);
          get.onerror = () => resolve(null);
        };
      }),
  );

/** Progress is saved a moment after it changes: wait until `ok` holds for the saved data. */
async function until(page, ok) {
  for (let i = 0; i < 30; i++) {
    if (ok(await idb(page))) return true;
    await page.waitForTimeout(200);
  }
  return false;
}

await t.launch();
try {
  const page = await t.newPage({ init: `localStorage.setItem("zth.tutor.v1", ${JSON.stringify(JSON.stringify({ apiKey: KEY, model: "claude-sonnet-5-5", maxTokens: 1024 }))});` });
  const calls = [];
  await page.route("https://api.anthropic.com/**", async (route) => {
    const req = route.request();
    if (req.method() === "OPTIONS") return route.fulfill({ status: 204, headers: CORS });
    calls.push(JSON.parse(req.postData() ?? "{}"));
    return route.fulfill({ status: 200, headers: { ...CORS, "content-type": "text/event-stream" }, body: sse("Nice and tidy. One thing to consider: names.") });
  });

  // ---- not offered before the lesson is passed
  await t.open(page, LESSON);
  check((await page.getByRole("button", { name: "Compare with reference" }).count()) === 0, "Compare with reference is not offered before the lesson is passed");

  // ---- pass it with the reference solution
  const sol = await t.solutionFiles(DIR);
  for (const [name, text] of Object.entries(sol)) await t.setFile(page, name, text);
  await t.runTests(page, /All tests passed/);
  const btn = page.getByRole("button", { name: "Compare with reference" });
  await btn.waitFor();
  check(true, "once the lesson is passed, Compare with reference appears");
  check(((await idb(page))?.lessons?.[LESSON]?.solutionRevealed ?? false) === false, "the solution has not been revealed yet");

  // ---- open it (the confirm is accepted by the harness)
  await btn.click();
  const cmp = page.locator(".compare");
  await cmp.waitFor();
  await page.locator(".compare .monaco-diff-editor").waitFor({ timeout: 30000 });
  check((await btn.getAttribute("aria-pressed")) === "true", "the button shows it is on");
  // (the harness types code with Monaco's auto-indent, so whitespace may differ from the reference even for the same code)
  check(/Every file matches|\d+ of \d+ files? differ/.test((await cmp.locator(".compare-bar").textContent()) ?? ""), "the bar says how many files match the reference");
  check(!(await page.locator(".editor-wrap").first().evaluate((e) => e.hidden)) || (await page.locator(".editor-wrap[hidden]").count()) === 1, "the normal editor is hidden while comparing");
  check(await until(page, (p) => p?.lessons?.[LESSON]?.solutionRevealed === true), "comparing counts as revealing the solution");
  await shot(page, "compare-1");

  // ---- a different solution shows differences
  await page.getByRole("button", { name: "Back to the editor" }).click();
  await page.locator(".compare").waitFor({ state: "detached" });
  check(await page.locator(".editor .monaco-editor").first().isVisible(), "Back to the editor returns to the code");
  const first = Object.keys(sol)[0];
  await t.setFile(page, first, sol[first] + "\n// my own note\n");
  await btn.click(); // already revealed: no confirm this time
  await cmp.waitFor();
  const bar = (await cmp.locator(".compare-bar").textContent()) ?? "";
  check(/1 of \d+ files? differ/.test(bar), `a changed file is reported (${bar.match(/\d+ of \d+ files? differ/)?.[0]})`);
  const tabs = await cmp.getByRole("tab").allTextContents();
  check(tabs.length === Object.keys(sol).length, `there is a tab per reference file (${tabs.join(", ")})`);
  await shot(page, "compare-2");
  const cur = await idb(page);
  check(cur.lessons[LESSON].status === "complete", "the lesson is still complete");

  // ---- the tutor review
  await cmp.getByRole("button", { name: "Ask the tutor to review mine" }).click();
  await page.locator(".tutor-reply", { hasText: "Nice and tidy" }).waitFor({ timeout: 15000 });
  check(calls.length === 1 && calls[0].system.includes("REVIEW") && !calls[0].system.includes("// my own note\n\n\n"), "the tutor is asked for a review");
  check(await until(page, (p) => (p?.lessons?.[LESSON]?.aiHelpUsed ?? 0) >= 1), "the review counts as tutor help");

  // ---- the toolbar button closes it again
  await btn.click();
  await cmp.waitFor({ state: "detached" });
  check(true, "pressing the button again closes the comparison");
  await page.close();
} catch (e) {
  t.crashed(e);
}
await t.finish();
