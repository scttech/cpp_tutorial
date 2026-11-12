// End-to-end checks for the AI tutor. The Anthropic API is mocked at the network layer (nothing real is
// called and no key is needed), so the checks are about OUR behaviour: what is sent, what is shown,
// what is counted, and what never leaves the browser.
//   usage: node tutor.mjs        env: APP_URL (default http://localhost:5173), SHOTS (screenshot dir)
import { createSuite } from "./lib.mjs";

const t = createSuite({ track: "01-foundations/" });
const { APP, check, shot } = t;
const HELLO = "core.foundations.hello-world";
const KEY = "sk-ant-test-0123456789abcdefghij";

await t.launch();

const CORS = {
  "access-control-allow-origin": "*",
  "access-control-allow-headers": "*",
  "access-control-allow-methods": "POST, OPTIONS",
};

const sse = (text, usage = { in: 120, out: 30 }) => {
  const ev = (type, data) => `event: ${type}\ndata: ${JSON.stringify({ type, ...data })}\n\n`;
  const mid = Math.ceil(text.length / 2);
  return (
    ev("message_start", { message: { usage: { input_tokens: usage.in, output_tokens: 1 } } }) +
    ev("content_block_delta", { delta: { type: "text_delta", text: text.slice(0, mid) } }) +
    ev("content_block_delta", { delta: { type: "text_delta", text: text.slice(mid) } }) +
    ev("message_delta", { delta: { stop_reason: "end_turn" }, usage: { output_tokens: usage.out } })
  );
};

/** Mock api.anthropic.com. `replies` is consumed in order (the last one repeats); a number is an HTTP error status. */
async function mockApi(page, replies) {
  const calls = [];
  await page.route("https://api.anthropic.com/**", async (route) => {
    const req = route.request();
    if (req.method() === "OPTIONS") return route.fulfill({ status: 204, headers: CORS });
    calls.push({ headers: req.headers(), body: JSON.parse(req.postData() ?? "{}") });
    const r = replies[Math.min(calls.length - 1, replies.length - 1)];
    if (typeof r === "number") return route.fulfill({ status: r, headers: { ...CORS, "content-type": "application/json" }, body: JSON.stringify({ error: { message: "mock failure" } }) });
    return route.fulfill({ status: 200, headers: { ...CORS, "content-type": "text/event-stream" }, body: sse(r) });
  });
  return calls;
}

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

const withKey = `localStorage.setItem("zth.tutor.v1", ${JSON.stringify(JSON.stringify({ apiKey: KEY, model: "claude-sonnet-5-5", maxTokens: 1024 }))});`;
const ask = async (page, mode, question) => {
  await page.getByLabel("Help mode").selectOption(mode);
  await page.getByLabel("Your question for the tutor").fill(question);
  await page.getByRole("button", { name: "Ask", exact: true }).click();
};
const openTutor = async (page) => {
  await page.locator(".left .tab", { hasText: "Tutor" }).click();
};

try {
  // ======================= no key: a clear invitation, nothing else =======================
  const bare = await t.newPage();
  await t.open(bare, HELLO);
  await openTutor(bare);
  check(((await bare.locator(".tutor").first().textContent()) ?? "").includes("never sees the solution"), "without a key the Tutor tab explains itself and says it never sees the solution");
  check((await bare.locator(".tutor a[href='#/settings']").count()) === 1, "…and points at Settings");
  await bare.close();

  // ======================= settings: the key is stored locally, masked, and never exported =======================
  const sp = await t.newPage({ acceptDownloads: true });
  await sp.goto(`${APP}/#/settings`);
  await sp.getByLabel("Anthropic API key").fill(KEY);
  await sp.getByRole("button", { name: "Save key" }).click();
  await sp.getByText("Key saved:").waitFor();
  const masked = (await sp.locator(".tutor-key code").first().textContent()) ?? "";
  check(!masked.includes("0123456789abcdefghij") && masked.startsWith("sk-ant-"), `the saved key is shown masked (${masked})`);
  check((await sp.evaluate(() => localStorage.getItem("zth.tutor.v1")))?.includes(KEY), "the key is stored in localStorage");
  const [dl] = await Promise.all([sp.waitForEvent("download"), sp.getByRole("button", { name: /Export progress/ }).click()]);
  const exported = await (await import("node:fs/promises")).readFile(await dl.path(), "utf8");
  check(!exported.includes("sk-ant") && !exported.includes(KEY), "the exported progress file contains no API key");
  await sp.reload();
  await sp.getByText("Key saved:").waitFor();
  check(true, "the key survives a reload");
  await shot(sp, "t1-settings");
  await sp.getByRole("button", { name: "Remove key" }).click();
  await sp.getByLabel("Anthropic API key").waitFor();
  check((await sp.evaluate(() => localStorage.getItem("zth.tutor.v1"))) === null, "Remove key clears it from storage");
  await sp.close();

  // ======================= a normal conversation =======================
  const page = await t.newPage({ init: withKey });
  const calls = await mockApi(page, ["What does `main` have to do to produce output? Where does text go in C++?"]);
  await t.open(page, HELLO);
  await openTutor(page);
  const sol = Object.values(await t.solutionFiles("01-hello-world")).join("\n");
  const secret = sol.split("\n").find((l) => l.includes("Hello, World"))?.trim() ?? "";
  check(secret.length > 10, `found the distinctive solution line to look for (${secret})`);

  check((await page.getByRole("option", { name: /Review/ }).isDisabled()) && (await page.getByRole("option", { name: /Quiz/ }).isDisabled()) && (await page.getByRole("option", { name: /Another one/ }).isDisabled()), "Review, Quiz and Another one like this are locked until the lesson is passed");
  check((await page.getByRole("button", { name: "Another one like this" }).count()) === 0, "the one-click practice button is not shown before the lesson is passed");
  const modes = await page.getByLabel("Help mode").locator("option").allTextContents();
  check(modes.length === 9 && modes.some((m) => m.startsWith("Discuss")), `nine modes including Discuss and practice (${modes.length})`);

  await ask(page, "nudge", "I'm lost. Where do I begin?");
  await page.locator(".tutor-reply", { hasText: "What does" }).waitFor({ timeout: 10000 });
  check(true, "the reply is shown");
  check(calls.length === 1, "one API call was made");
  const c = calls[0];
  check(c.headers["x-api-key"] === KEY && c.headers["anthropic-dangerous-direct-browser-access"] === "true" && c.headers["anthropic-version"] === "2023-06-01", "the request carries the key, the version and the browser-access header");
  check(c.body.model === "claude-sonnet-5-5" && c.body.stream === true && c.body.max_tokens === 1024, "model, streaming and max_tokens are as configured");
  check(c.body.system.includes("NUDGE") && c.body.system.includes("Hello, World") && c.body.system.includes("TODO"), "the system prompt has the mode, the lesson and the learner's code");
  const wire = JSON.stringify(c.body);
  check(!/test\.cpp|ZTH_TEST|ZTH_EXPECT/.test(wire), "no hidden test source is in anything sent");
  check(c.body.messages.at(-1).content === "I'm lost. Where do I begin?" && c.body.messages.length === 1, "the learner's question is the only message of the first turn");
  check(((await page.locator(".tutor-form").textContent()) ?? "").includes("120 in · 30 out"), "token usage is shown (120 in · 30 out)");
  check(((await page.locator(".tutor-form").textContent()) ?? "").includes("1 used"), "the request counted as AI help (1 used)");
  await shot(page, "t2-reply");

  // a follow-up carries the conversation
  await ask(page, "discuss", "Why isn't printing built into the language?");
  await page.locator(".turn.user", { hasText: "Why isn't printing" }).waitFor();
  await page.waitForFunction(() => document.querySelectorAll(".tutor-reply").length >= 2);
  check(calls.length === 2 && calls[1].body.messages.length === 3, "the follow-up sends the earlier turns (3 messages)");
  check(calls[1].body.system.includes("DISCUSS") && calls[1].body.system.includes("never state the answer"), "Discuss mode is sent with its Socratic rules");

  // progress records the help, and the exported data has no key
  await page.waitForTimeout(500);
  const p = await idb(page);
  check(p?.lessons?.[HELLO]?.aiHelpUsed === 2 && p?.lessons?.[HELLO]?.explainUsed === true, "progress records aiHelpUsed = 2 and explainUsed (Discuss counts)");

  // the chat survives switching tabs
  await page.locator(".left .tab", { hasText: "Hints" }).click();
  await openTutor(page);
  check((await page.locator(".turn.user").count()) === 2, "the conversation survives switching tabs");
  await page.getByRole("button", { name: "Clear chat" }).click();
  check((await page.locator(".turn.user").count()) === 0, "Clear chat empties it");

  // ======================= nothing of the reference solution is sent =======================
  // (lines the lesson text, the starter or the hints already show are not secrets, so they are excluded)
  {
    const { readFile } = await import("node:fs/promises");
    const { fileURLToPath } = await import("node:url");
    const dir = new URL("../../content/tracks/core/01-foundations/03-operators-and-integers/", import.meta.url);
    const read = (f) => readFile(fileURLToPath(new URL(f, dir)), "utf8").catch(() => "");
    const known = (await Promise.all(["lesson.md", "fast.md", "hints.yaml", "starter/main.cpp"].map(read))).join("\n");
    const secrets = (await read("solution/main.cpp"))
      .split("\n")
      .map((l) => l.trim())
      .filter((l) => l.length >= 20 && !known.includes(l));
    const o = await t.newPage({ init: withKey });
    const ocalls = await mockApi(o, ["Think about overflow."]);
    await t.open(o, "core.foundations.operators-and-integers");
    await openTutor(o);
    await ask(o, "pinpoint", "Where is my bug?");
    await o.locator(".tutor-reply").first().waitFor({ timeout: 10000 });
    const sent = JSON.stringify(ocalls[0].body);
    check(secrets.length >= 5, `found ${secrets.length} solution lines that appear nowhere in the lesson, starter or hints`);
    const leaked = secrets.filter((l) => sent.includes(JSON.stringify(l).slice(1, -1)));
    check(leaked.length === 0, `none of them is in what was sent to the API${leaked.length ? ": " + leaked[0] : ""}`);
    await o.close();
  }

  // ======================= Explain this error =======================
  await t.setCode(page, '#include <iostream>\nint main() {\n    std::cout << "Hi"\n}\n');
  await page.getByRole("button", { name: /Run Tests/ }).click();
  await page.locator(".results .panel-body", { hasText: "did not compile" }).first().waitFor({ timeout: 60000 });
  await page.getByRole("tab", { name: /Problems/ }).click();
  await page.getByRole("button", { name: /Explain this error/ }).first().click();
  await page.locator(".tutor-reply").first().waitFor({ timeout: 10000 });
  check((await page.locator(".left .tab.on").textContent())?.includes("Tutor"), "Explain this error opens the Tutor tab");
  const last = calls.at(-1).body;
  check(last.system.includes("EXPLAIN MY ERROR") && /error:/.test(last.system) && last.messages.at(-1).content.includes("do not write the fix"), "…in 'Explain my error' mode with the compiler message in the context");
  await shot(page, "t3-explain-error");

  // ======================= the guardrail =======================
  const leaky = "Sure:\n```cpp\n#include <iostream>\nint main() {\n    " + secret + "\n}\n```";
  const g = await t.newPage({ init: withKey });
  const gcalls = await mockApi(g, [leaky]);
  await t.open(g, HELLO);
  await openTutor(g);
  await ask(g, "approach", "Just show me.");
  await g.locator(".tutor-reply.fell-back").waitFor({ timeout: 10000 });
  check(gcalls.length === 2, "a reply that reproduces the solution is re-asked once");
  check((gcalls[1].body.messages.at(-1).content ?? "").includes("without writing solution code"), "…with the stricter instruction");
  const shown = (await g.locator(".tutor-turns").textContent()) ?? "";
  check(!shown.includes(secret) && shown.includes("can't answer that without handing you the solution"), "the leaked reply is never shown; the learner gets the safe message");
  const leaks = await g.evaluate(() => JSON.parse(localStorage.getItem("zth.tutor.leaks.v1") ?? "[]"));
  check(leaks.length === 2 && leaks[0].reason === "matches-solution", "both catches are logged locally");
  await shot(g, "t4-guardrail");
  await g.close();

  // a long block in a hint mode is also caught, a review is not
  const long = "```cpp\n" + Array.from({ length: 15 }, (_, i) => `int v${i} = ${i};`).join("\n") + "\n```";
  const l = await t.newPage({ init: withKey });
  const lcalls = await mockApi(l, [long, "Fine now: think about streams."]);
  await t.open(l, HELLO);
  await openTutor(l);
  await ask(l, "concept", "Explain streams.");
  await l.locator(".tutor-reply", { hasText: "think about streams" }).waitFor({ timeout: 10000 });
  check(lcalls.length === 2 && ((await l.locator(".tutor-turns").textContent()) ?? "").includes("think about streams") && !((await l.locator(".tutor-turns").textContent()) ?? "").includes("int v14"), "a 15-line block is caught and replaced by the clean second answer");
  await l.close();

  // ======================= API failures =======================
  const f = await t.newPage({ init: withKey });
  await mockApi(f, [401]);
  await t.open(f, HELLO);
  await openTutor(f);
  await ask(f, "nudge", "Help");
  await f.locator(".tutor .banner.error").waitFor({ timeout: 10000 });
  check(((await f.locator(".tutor .banner.error").textContent()) ?? "").includes("key was rejected"), "a 401 says the key was rejected");
  check((await f.locator(".turn.user").count()) === 0, "the unanswered question is taken back out of the chat");
  check((await f.getByLabel("Your question for the tutor").inputValue()) === "Help", "…and stays in the box so it can be re-sent");
  await f.waitForTimeout(400);
  check(((await idb(f))?.lessons?.[HELLO]?.aiHelpUsed ?? 0) === 0, "a failed request is not counted as AI help");
  await f.close();

  // ======================= Good Question =======================
  const q = await t.newPage({ init: withKey });
  const qcalls = await mockApi(q, ["Think about what a stream is."]);
  await t.open(q, HELLO);
  await openTutor(q);
  await ask(q, "concept", "What is a stream?");
  await q.locator(".tutor-reply").first().waitFor({ timeout: 10000 });
  const files = await t.solutionFiles("01-hello-world");
  for (const [name, text] of Object.entries(files)) await t.setFile(q, name, text);
  const b = await t.runTests(q, /All tests passed/);
  check(/All tests passed/.test(b), `the solution passes (${b})`);
  await q.waitForTimeout(500);
  const done = await idb(q);
  check(done?.events?.["good-question"]?.count === 1 && done?.badges?.["good-question"], "explain, then pass without revealing: the Good Question badge is earned");
  check((await q.getByRole("option", { name: /Review/ }).isEnabled()) && (await q.getByRole("option", { name: /Quiz/ }).isEnabled()) && (await q.getByRole("option", { name: /Another one/ }).isEnabled()), "Review, Quiz and Another one like this unlock once the lesson is passed");
  const before = qcalls.length;
  await q.getByRole("button", { name: "Another one like this" }).click();
  await q.locator(".tutor-reply", { hasText: "stream" }).nth(1).waitFor({ timeout: 10000 });
  check(qcalls.length === before + 1 && qcalls.at(-1).body.system.includes("PRACTICE") && qcalls.at(-1).body.system.includes("Do NOT write a solution"), "the practice button asks the tutor for a fresh problem, and forbids a solution");
  await q.close();

  // using the tutor costs the Self-Reliant badge, as hints do
  const sr = await t.newPage({ init: withKey });
  await mockApi(sr, ["Look at the starter comment."]);
  await t.open(sr, HELLO);
  await openTutor(sr);
  await ask(sr, "nudge", "Hint please");
  await sr.locator(".tutor-reply").first().waitFor({ timeout: 10000 });
  for (const [name, text] of Object.entries(files)) await t.setFile(sr, name, text);
  await t.runTests(sr, /All tests passed/);
  await sr.waitForTimeout(500);
  const srp = await idb(sr);
  check(!srp?.badges?.["self-reliant-bronze"] && !srp?.badges?.["good-question"], "a Nudge costs Self-Reliant and does not earn Good Question");
  await sr.close();
} catch (e) {
  t.crashed(e);
}

await t.finish();
