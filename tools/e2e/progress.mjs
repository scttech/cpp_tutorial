// End-to-end checks for friendly errors, badges, and progress persistence (IndexedDB, export/import,
// migration). Each scenario uses a brand-new browser context, so it starts with empty storage.
//   usage: node progress.mjs        env: APP_URL (default http://localhost:5173), SHOTS (screenshot dir)
import { readFile } from "node:fs/promises";
import { fileURLToPath } from "node:url";
import { createSuite } from "./lib.mjs";

const t = createSuite({ track: "01-foundations/" });
const { APP, check, shot } = t;
const M1 = new URL("../../content/tracks/core/01-foundations/", import.meta.url);
const solution = (dir, file) => readFile(fileURLToPath(new URL(`${dir}/solution/${file}`, M1)), "utf8");
const HELLO = "core.foundations.hello-world";


await t.launch();

const newPage = (opts = {}) => t.newPage({ ...opts, acceptDownloads: true });
const openLesson = (page, id) => t.open(page, id);
const setCode = (page, text) => t.setCode(page, text);
const click = async (page, name, waitFor) => {
  await page.getByRole("button", { name }).click();
  await page.locator(".results .panel-body", { hasText: waitFor }).first().waitFor({ timeout: 40000 });
};
const idbProgress = (page) =>
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

try {
  // ======================= friendly errors + badges + persistence =======================
  const page = await newPage();
  await openLesson(page, HELLO);

  // --- friendly compile error (missing semicolon) ---
  await setCode(page, '#include <iostream>\nint main() {\n    std::cout << "Hello, World!\\n"\n}\n');
  await click(page, /Run Tests/, "did not compile");
  check(((await page.locator(".start-here").textContent()) ?? "").includes("missing its closing semicolon"), "friendly explanation shown on the Tests tab for the first error");
  await page.getByRole("tab", { name: /Problems/ }).click();
  const ptext = (await page.locator(".prob .ptext").first().textContent()) ?? "";
  check(ptext.includes("semicolon") && !ptext.includes("expected ';'"), "Problems tab leads with the friendly text, not the raw message");
  await page.locator(".raw summary").first().click();
  check(((await page.locator(".raw code").first().textContent()) ?? "").includes("expected ';'"), "the raw compiler message is still available (expandable)");
  check((await page.locator(".learn-more").count()) >= 1, "friendly error links to the relevant lesson");
  await shot(page, "p1-friendly");

  // --- friendly linker error (no line number) ---
  await setCode(page, "int f(int);\nint main() { return f(1); }\n");
  await page.getByRole("tab", { name: /Tests/ }).click();
  await click(page, /Run Tests/, "did not compile");
  await page.getByRole("tab", { name: /Problems/ }).click();
  await page.locator(".problems", { hasText: /linker|undefined/i }).first().waitFor({ timeout: 30000 }); // wait for THIS run, not the previous one
  const linkText = (await page.locator(".problems").textContent()) ?? "";
  check(/linker can.t find a definition/.test(linkText), "linker error is explained in plain language");
  await page.locator(".prob").first().click(); // line 0: must not throw
  check(true, "clicking a linker problem (no line number) does not break");

  // --- fix it: pass, earn badges ---
  await setCode(page, await solution("01-hello-world", "main.cpp"));
  await page.getByRole("tab", { name: /Tests/ }).click();
  await click(page, /Run Tests/, "All tests passed");
  await page.locator(".toast", { hasText: "Hello, World" }).first().waitFor({ timeout: 10000 });
  check(true, "a toast announces the Hello, World badge");
  check((await page.locator(".toast", { hasText: "It Works!" }).count()) >= 1, "…and the It Works! bronze badge");
  check(((await page.locator(".nav .count").textContent()) ?? "") === String(await page.locator(".toast").count()), "the nav shows the number of badges earned");
  await shot(page, "p2-toast");

  // --- profile page ---
  await page.getByRole("link", { name: /^Profile/ }).click();
  await page.locator(".badge-card.earned").first().waitFor();
  const earned = await page.locator(".badge-card.earned .bname").allTextContents();
  check(earned.some((t) => t.includes("Hello, World")) && earned.some((t) => t.includes("It Works!")), `earned badges are shown in colour (${earned.map((t) => t.trim()).join(", ")})`);
  const hidden = await page.locator(".badge-card.locked .bname", { hasText: "???" }).count();
  check(hidden >= 3, `hidden badges stay secret until earned (${hidden} shown as ???)`);
  check((await page.locator("details.soon").count()) === 0, "every badge refers to content that exists now, so none is grouped as 'coming soon' (the grouping itself is covered by the unit tests)");
  await page.locator(".badge-card.earned", { hasText: "Hello, World" }).first().click();
  check(((await page.locator(".badge-detail").textContent()) ?? "").includes("Hello, World (and what just happened)"), "badge detail lists the evidence (which lesson counted)");
  await shot(page, "p3-profile");

  // --- persistence is IndexedDB, survives reload ---
  const saved = await idbProgress(page);
  check(saved?.lessons?.[HELLO]?.status === "complete" && !!saved?.badges?.["hello-world"], "progress is stored in IndexedDB");
  check((await page.evaluate(() => localStorage.getItem("zth.progress.v1") ?? localStorage.getItem("zth.progress.v2"))) === null, "…and not in localStorage");
  await page.waitForTimeout(400);
  await page.reload();
  await page.getByRole("link", { name: /^Profile/ }).waitFor();
  await page.goto(`${APP}/#/`);
  await page.getByText("✔ complete").first().waitFor();
  check(true, "progress survives a full reload");
  check(((await page.locator(".nav .count").textContent()) ?? "") !== "", "badges survive a reload too");

  // --- segfault: a hidden badge ---
  await openLesson(page, "core.foundations.variables-and-types");
  await setCode(page, "int main() { int* p = nullptr; *p = 1; }\n");
  await page.getByRole("button", { name: /^▶ Run$/ }).click();
  await page.locator(".toast", { hasText: "Segfault Survivor" }).first().waitFor({ timeout: 30000 });
  check(true, "crashing with SIGSEGV earns the hidden Segfault Survivor badge");
  await page.goto(`${APP}/#/profile`);
  check((await page.locator(".badge-card.earned", { hasText: "Segfault Survivor" }).count()) === 1, "the hidden badge is revealed on the profile once earned");

  // --- settings: export ---
  await page.goto(`${APP}/#/settings`);
  const [download] = await Promise.all([page.waitForEvent("download"), page.getByRole("button", { name: /Export progress/ }).click()]);
  const exportPath = await download.path();
  const exported = JSON.parse(await readFile(exportPath, "utf8"));
  check(exported.app === "cpp-zero-to-hero" && exported.progress?.badges?.["hello-world"], "export downloads a JSON file with progress and badges");
  check(!/api[_-]?key|sk-ant/i.test(JSON.stringify(exported)), "the export contains no secrets");

  // --- reset, then import restores everything ---
  await page.getByRole("button", { name: /Erase all progress/ }).click();
  await page.getByText("All progress erased.").waitFor();
  await page.goto(`${APP}/#/profile`);
  check((await page.locator(".badge-card.earned").count()) === 0, "reset removes all badges");
  const afterReset = await idbProgress(page);
  check(afterReset === null, "reset clears the IndexedDB record");
  await page.goto(`${APP}/#/settings`);
  await page.locator('input[type="file"]').setInputFiles(exportPath);
  await page.getByRole("button", { name: /Replace my progress/ }).click();
  await page.getByText(/Imported \d+ completed lessons/).waitFor();
  await page.goto(`${APP}/#/profile`);
  check((await page.locator(".badge-card.earned", { hasText: "Hello, World" }).count()) === 1, "importing the export restores the badges");
  await page.goto(`${APP}/#/`);
  await page.getByText("✔ complete").first().waitFor();
  check(true, "…and the completed lessons");

  // --- importing a bad file is rejected without touching progress ---
  await page.goto(`${APP}/#/settings`);
  await page.locator('input[type="file"]').setInputFiles({ name: "bad.json", mimeType: "application/json", buffer: Buffer.from('{"hello":"world"}') });
  await page.getByText(/does not look like a progress export/).waitFor();
  check(true, "a file that is not a progress export is rejected with a clear message");
  await page.locator('input[type="file"]').setInputFiles({ name: "bad2.json", mimeType: "application/json", buffer: Buffer.from("not json at all") });
  await page.getByText(/not valid JSON/).waitFor();
  check(true, "invalid JSON is rejected");
  await page.goto(`${APP}/#/profile`);
  check((await page.locator(".badge-card.earned", { hasText: "Hello, World" }).count()) === 1, "rejected imports leave existing progress intact");

  // --- pace: fast opens the summary first ---
  await page.goto(`${APP}/#/settings`);
  await page.getByLabel(/Fast: the short summary first/).check();
  await openLesson(page, "core.foundations.control-flow");
  check((await page.locator(".left .tab.on").textContent())?.includes("Fast summary"), "Fast pace opens lessons on the fast summary");
  await page.close();

  // ======================= migration from the first release =======================
  const legacy = {
    version: 1,
    lessons: {
      [HELLO]: { status: "complete", bestTiers: { works: true, robust: false, fast: false, clean: false }, attempts: 5, testRuns: 3, hintsUsed: 0, solutionRevealed: false, lastCode: { "main.cpp": "// my old code" }, firstCompleteAt: "2026-01-01T10:00:00.000Z" },
    },
  };
  const old = await newPage({ init: `if (!localStorage.getItem("zth.migrated-test")) { localStorage.setItem("zth.progress.v1", ${JSON.stringify(JSON.stringify(legacy))}); localStorage.setItem("zth.migrated-test", "1"); }` });
  await old.goto(`${APP}/#/`);
  await old.getByText("✔ complete").first().waitFor();
  check(true, "progress from the first release (localStorage) is migrated and shown");
  await old.waitForTimeout(500);
  const migrated = await idbProgress(old);
  check(migrated?.lessons?.[HELLO]?.lastCode?.["main.cpp"] === "// my old code", "…including the saved code, now in IndexedDB");
  check((await old.evaluate(() => localStorage.getItem("zth.progress.v1"))) === null, "…and the legacy localStorage key is removed");
  await old.goto(`${APP}/#/profile`);
  await old.locator(".badge-card.earned", { hasText: "Hello, World" }).first().waitFor({ timeout: 10000 });
  check(true, "badges are awarded retroactively for migrated progress (without toasts)");
  await shot(old, "p4-migrated-profile");
  await old.close();
} catch (e) {
  t.crashed(e);
}

await t.finish();
