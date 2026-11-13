// End-to-end checks for flagging (a note on a lesson or a warm-up card, kept with the code and the last result, exported as Markdown) and for
// the backup reminder on Home. No Docker needed.
//   usage: node flags.mjs        env: APP_URL (default http://localhost:5173), SHOTS (screenshot dir)
import { readFile } from "node:fs/promises";
import { createSuite } from "./lib.mjs";

const t = createSuite({ track: "02-data-and-memory/" });
const { APP, check, shot } = t;
const ARRAYS = "core.data-and-memory.arrays";

const readIdb = (page) =>
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
const editIdb = (page, fn) =>
  page.evaluate(
    (src) =>
      new Promise((resolve) => {
        const open = indexedDB.open("zth", 1);
        open.onsuccess = () => {
          const store = open.result.transaction("kv", "readwrite").objectStore("kv");
          const get = store.get("progress");
          get.onsuccess = () => {
            const p = get.result;
            new Function("p", src)(p);
            store.put(p, "progress").onsuccess = () => resolve(true);
          };
        };
      }),
    fn.toString().replace(/^[^{]*\{|\}$/g, ""),
  );
const until = async (page, ok) => {
  for (let i = 0; i < 40; i++) {
    if (ok(await readIdb(page))) return true;
    await page.waitForTimeout(200);
  }
  return false;
};
const today = () => {
  const d = new Date();
  return `${d.getFullYear()}-${String(d.getMonth() + 1).padStart(2, "0")}-${String(d.getDate()).padStart(2, "0")}`;
};

await t.launch();
try {
  // ======================= flags =======================
  const page = await t.newPage({ acceptDownloads: true });
  await t.open(page, ARRAYS);
  const flag = page.getByRole("button", { name: "Flag", exact: true });
  await flag.click();
  const dialog = page.getByRole("dialog", { name: /Flag this lesson/ });
  await dialog.waitFor();
  check(await dialog.getByRole("button", { name: "Save flag" }).isDisabled(), "Save flag is disabled until there is a note");
  await page.keyboard.press("Escape");
  await dialog.waitFor({ state: "detached" });
  check(true, "Escape closes the form");

  await flag.click();
  await dialog.getByLabel(/What is wrong/).fill("The second test name is unclear.");
  await shot(page, "flag-1");
  await dialog.getByRole("button", { name: "Save flag" }).click();
  await page.getByRole("status").filter({ hasText: "Flagged. 1 saved." }).waitFor();
  check(true, "saving announces 'Flagged. 1 saved.'");
  await until(page, (p) => p?.flags?.length === 1);
  const f1 = (await readIdb(page)).flags[0];
  check(f1.note === "The second test name is unclear." && f1.target.id === ARRAYS && f1.target.type === "lesson", "the flag is saved with its note and lesson");
  check(/TODO/.test(f1.code?.["main.cpp"] ?? ""), "it includes the current code (the starter, with its TODO)");
  check(/No run yet/.test(f1.result ?? "") && /Docker|WASM|browser/i.test(f1.context ?? ""), `and the last result and the runner (${f1.context})`);

  // without the code
  await flag.click();
  await dialog.getByLabel(/What is wrong/).fill("Typo in the lesson text.");
  await dialog.getByLabel(/Include my code/).uncheck();
  await dialog.getByRole("button", { name: "Save flag" }).click();
  await until(page, (p) => p?.flags?.length === 2);
  const f2 = (await readIdb(page)).flags[0];
  check(f2.note.startsWith("Typo") && f2.code === undefined && f2.result === undefined, "unticking 'Include my code' leaves the code and result out");

  // a warm-up card
  await page.goto(`${APP}/#/warmup`);
  await page.locator(".warm-prompt").waitFor();
  await page.getByRole("button", { name: "Flag this card" }).click();
  await page.getByRole("dialog", { name: /Flag this card/ }).getByLabel(/What is wrong/).fill("This answer looks wrong to me.");
  await page.getByRole("dialog").getByRole("button", { name: "Save flag" }).click();
  await until(page, (p) => p?.flags?.length === 3);
  const f3 = (await readIdb(page)).flags[0];
  check(f3.target.type === "card" && /.+/.test(f3.context ?? ""), `a card flag keeps the card and its question (${(f3.context ?? "").slice(0, 50)})`);

  // settings: list, export, delete
  await page.goto(`${APP}/#/settings`);
  await page.getByRole("heading", { name: "Flagged issues" }).waitFor();
  const items = page.locator(".flag-list li");
  check((await items.count()) === 3 && /Warm-up card/.test((await items.nth(0).textContent()) ?? "") && /Arrays/.test((await items.nth(2).textContent()) ?? ""), "Settings lists the flags, newest first, with the lesson's title");
  const [dl] = await Promise.all([page.waitForEvent("download"), page.getByRole("button", { name: "Export flags (.md)" }).click()]);
  check(/^zth-flags-\d{4}-\d{2}-\d{2}\.md$/.test(dl.suggestedFilename()), `the export is a Markdown file (${dl.suggestedFilename()})`);
  const md = await readFile(await dl.path(), "utf8");
  check(md.includes("# Flagged issues") && md.includes("3 flags.") && md.includes("The second test name is unclear.") && md.includes("This answer looks wrong to me."), "it contains the title, the count and every note");
  check(md.indexOf("The second test name") < md.indexOf("Typo in the lesson text") && md.indexOf("Typo in the lesson text") < md.indexOf("This answer looks wrong"), "the notes are oldest first");
  check(/```cpp\n[\s\S]*TODO[\s\S]*```/.test(md) && md.includes(`\`${ARRAYS}\``), "the code is in a fenced block and the lesson id is named");
  await items.nth(0).getByRole("button", { name: /Delete the flag/ }).click();
  check((await items.count()) === 2, "Delete removes one flag");
  await until(page, (p) => p?.flags?.length === 2); // saved a moment after it changes
  await page.reload();
  await page.getByRole("heading", { name: "Flagged issues" }).waitFor();
  check((await page.locator(".flag-list li").count()) === 2, "flags survive a reload");
  await page.getByRole("button", { name: "Delete all" }).click(); // the harness accepts the confirm
  await page.getByText("No flags yet.").waitFor();
  check(true, "Delete all clears them");
  await page.close();

  // ======================= backup reminder =======================
  const b = await t.newPage({ acceptDownloads: true });
  await b.goto(`${APP}/#/`);
  await b.locator(".plan-card").waitFor();
  check((await b.locator(".backup-card").count()) === 0, "a new learner gets no backup reminder");
  await until(b, (p) => !!p?.plan); // the first save happens a moment after the page loads
  await editIdb(b, () => {
    const d = new Date(Date.now() - 10 * 86400000);
    const key = `${d.getFullYear()}-${String(d.getMonth() + 1).padStart(2, "0")}-${String(d.getDate()).padStart(2, "0")}`;
    p.lessons["core.foundations.hello-world"] = { status: "complete", firstCompleteAt: d.toISOString() };
    p.activity[key] = { warmups: 0, resolves: 0, lessons: 1, runs: 3, ms: 0 };
  });
  await b.reload();
  const card = b.locator(".backup-card");
  await card.waitFor();
  check(/not exported it yet/.test((await card.textContent()) ?? ""), "after 10 days of progress and no export, Home asks for a backup");
  await shot(b, "flag-2-backup");

  await card.getByRole("button", { name: "Remind me in a week" }).click();
  check((await b.locator(".backup-card").count()) === 0, "Remind me in a week hides the card");
  await b.reload();
  await b.locator(".plan-card").waitFor();
  check((await b.locator(".backup-card").count()) === 0, "and it stays hidden after a reload");
  const stored = JSON.parse(await b.evaluate(() => localStorage.getItem("zth.backup.v1")));
  check(!!stored.snoozedUntil && stored.snoozedUntil > today(), `the snooze is saved (until ${stored.snoozedUntil})`);

  await b.evaluate(() => localStorage.removeItem("zth.backup.v1"));
  await b.reload();
  await card.waitFor();
  const [dl2] = await Promise.all([b.waitForEvent("download"), b.locator(".backup-card").getByRole("button", { name: "Export now" }).click()]);
  check(/^zth-progress-/.test(dl2.suggestedFilename()), "Export now downloads the progress file");
  await b.locator(".backup-card").waitFor({ state: "detached" });
  check(JSON.parse(await b.evaluate(() => localStorage.getItem("zth.backup.v1"))).lastExport === today(), "the export is remembered, and the card goes away");
  await b.goto(`${APP}/#/settings`);
  await b.getByText(/Last exported: \d{4}-\d{2}-\d{2}/).waitFor();
  check(true, "Settings shows the date of the last export");
  await b.close();
} catch (e) {
  t.crashed(e);
}
await t.finish();
