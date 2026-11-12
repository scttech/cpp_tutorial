// End-to-end checks for re-solving: a passed lesson comes due, the re-solve starts from the starter code without touching the saved solution,
// a hint is a miss (back to 3 days), a clean re-solve climbs to 7 days, giving up is a miss, and a failure shows up as a weak spot.
// Needs the Docker runner (a lesson is really solved).
//   usage: node resolve.mjs        env: APP_URL (default http://localhost:5173), SHOTS (screenshot dir)
import { createSuite } from "./lib.mjs";

const t = createSuite({ track: "02-data-and-memory/" });
const { APP, check, shot } = t;
const ID = "core.data-and-memory.arrays";
const DIR = "01-arrays-and-std-array";

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
/** Rewrite the saved progress with `fn` (run in the page), e.g. to move a date into the past. */
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
  for (let i = 0; i < 30; i++) {
    if (ok(await readIdb(page))) return true;
    await page.waitForTimeout(200);
  }
  return false;
};
const editorText = (page) => page.locator(".monaco-editor .view-lines").first().innerText();

await t.launch();
try {
  const page = await t.newPage();
  await t.solve(page, ID, DIR, "arrays");
  await until(page, (p) => p?.lessons?.[ID]?.status === "complete");
  const before = await readIdb(page);
  const savedCode = before.lessons[ID].lastCode["main.cpp"];
  check(!/TODO/.test(savedCode), "the saved solution has no TODO");

  // ---- not due yet: no chip; a lesson passed 10 days ago is due
  await page.goto(`${APP}/#/`);
  await page.getByLabel("Search lessons, concepts and gotchas").fill("arrays");
  await page.locator(".results-list").waitFor();
  check((await page.locator(".due-chip").count()) === 0, "a lesson passed just now is not due for a re-solve");
  await editIdb(page, () => {
    const l = p.lessons["core.data-and-memory.arrays"];
    l.firstCompleteAt = new Date(Date.now() - 10 * 86400000).toISOString();
    delete p.resolves?.["core.data-and-memory.arrays"];
  });
  await page.reload();
  await page.getByLabel("Search lessons, concepts and gotchas").fill("arrays");
  const chip = page.locator(".results-list .due-chip").first();
  await chip.waitFor();
  check(/re-solve due/.test((await chip.textContent()) ?? ""), "a lesson passed 10 days ago shows 're-solve due' on Home");
  await shot(page, "resolve-1-home");

  // ---- the banner on the lesson page, then start the re-solve
  await page.goto(`${APP}/#/lesson/${ID}`);
  await page.locator(".monaco-editor").first().waitFor({ timeout: 30000 });
  check(((await page.locator(".banner").first().textContent()) ?? "").includes("due for a re-solve"), "the lesson page says it is due for a re-solve");
  check(!/TODO/.test(await editorText(page)), "the normal lesson page still shows the saved solution");
  await page.getByRole("link", { name: "Re-solve", exact: true }).click();
  await page.locator(".resolve-bar").waitFor();
  await page.locator(".monaco-editor").first().waitFor();
  await page.waitForFunction(() => /TODO/.test(document.querySelector(".monaco-editor .view-lines")?.textContent ?? ""));
  check(true, "the re-solve starts from the starter code (TODO is back)");
  check((await page.getByRole("button", { name: "Compare with reference" }).count()) === 0, "Compare is not offered during a re-solve");
  await shot(page, "resolve-2-start");

  // ---- a hint is recorded locally only
  await page.getByRole("tab", { name: "Hints" }).click();
  await page.getByRole("button", { name: /Reveal level 1/ }).click();
  const hintBar = (await page.locator(".resolve-bar").textContent()) ?? "";
  check(/1 hint(?!s)/.test(hintBar), `the bar counts the hint (${hintBar.slice(-40)})`);
  const afterHint = await readIdb(page);
  check(afterHint.lessons[ID].hintsUsed === before.lessons[ID].hintsUsed, "the hint did not change the lesson's saved hint count");

  // ---- solve it (with the hint used) -> counts as a miss
  const sol = await t.solutionFiles(DIR);
  for (const [name, text] of Object.entries(sol)) await t.setFile(page, name, text);
  await t.runTests(page, /All tests passed/);
  await page.locator(".resolve-bar [role=status]").waitFor();
  const msg1 = (await page.locator(".resolve-bar").textContent()) ?? "";
  check(/counts as a miss/.test(msg1) && /Next re-solve in 3 days/.test(msg1), `passing with a hint counts as a miss, next in 3 days (${msg1.slice(0, 120)})`);
  await until(page, (p) => p?.resolves?.[ID]?.count === 1);
  let p1 = await readIdb(page);
  check(p1.resolves[ID].box === 1 && p1.resolves[ID].misses === 1, "saved: box 1, one miss");
  check(p1.weak?.[`lesson:${ID}`]?.misses === 1, "the miss is in the weak-spot tally");
  check(p1.lessons[ID].lastCode["main.cpp"] === savedCode && p1.lessons[ID].attempts === before.lessons[ID].attempts && p1.lessons[ID].testRuns === before.lessons[ID].testRuns, "the saved solution and the attempt counts are untouched");

  // ---- the failure shows as a weak spot on Home (this lesson has warm-up cards or is re-solved)
  await page.goto(`${APP}/#/`);
  await page.locator(".weak-card").waitFor();
  check((await page.locator(".weak-card .weak-list li", { hasText: "Arrays" }).count()) >= 1, "the missed lesson is listed under Weak spots");

  // ---- a second, clean re-solve climbs to 7 days
  await page.goto(`${APP}/#/lesson/${ID}?resolve=1`);
  await page.locator(".resolve-bar").waitFor();
  await page.waitForFunction(() => /TODO/.test(document.querySelector(".monaco-editor .view-lines")?.textContent ?? ""));
  for (const [name, text] of Object.entries(sol)) await t.setFile(page, name, text);
  await t.runTests(page, /All tests passed/);
  await page.locator(".resolve-bar [role=status]").waitFor();
  const msg2 = (await page.locator(".resolve-bar").textContent()) ?? "";
  check(/Clean re-solve/.test(msg2) && /Next re-solve in 7 days/.test(msg2), `a clean re-solve moves to 7 days (${msg2.slice(0, 100)})`);
  await shot(page, "resolve-3-clean");
  await until(page, (p) => p?.resolves?.[ID]?.count === 2);
  const p2 = await readIdb(page);
  check(p2.resolves[ID].box === 2 && p2.resolves[ID].bestMs > 0, "saved: box 2 and a best time");
  check(p2.weak[`lesson:${ID}`].run === 1, "the clean re-solve counts as one right answer toward clearing the weak spot");
  await page.getByRole("button", { name: "Run Tests" }).click();
  await page.waitForTimeout(1500);
  check((await readIdb(page)).resolves[ID].count === 2, "running the tests again after finishing does not record a second time");

  // ---- giving up is a miss (leave the finished page first: the same address would not start a new session)
  await page.goto(`${APP}/#/`);
  await page.goto(`${APP}/#/lesson/${ID}?resolve=1`);
  await page.locator(".resolve-bar").waitFor();
  await page.getByRole("button", { name: "Give up" }).click();
  await page.locator(".resolve-bar [role=status]", { hasText: "Counted as a miss" }).waitFor();
  await until(page, (p) => p?.resolves?.[ID]?.count === 3);
  check((await readIdb(page)).resolves[ID].box === 1, "giving up is a miss: back to box 1");

  // ---- a lesson that is not complete cannot be re-solved
  await page.goto(`${APP}/#/lesson/core.foundations.hello-world?resolve=1`);
  await page.locator(".monaco-editor").first().waitFor({ timeout: 30000 });
  check((await page.locator(".resolve-bar").count()) === 0, "a lesson that has not been passed ignores ?resolve=1");
  await page.close();
} catch (e) {
  t.crashed(e);
}
await t.finish();
