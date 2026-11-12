// End-to-end checks for the learning stats page, and for the recording behind it (time on a lesson, the compiler errors hit, the time to the
// first pass). Part 1 seeds progress in IndexedDB; part 2 really runs code, so it needs the Docker runner.
//   usage: node stats.mjs        env: APP_URL (default http://localhost:5173), SHOTS (screenshot dir)
import AxeBuilder from "@axe-core/playwright";
import { createSuite } from "./lib.mjs";

const t = createSuite({ track: "02-data-and-memory/" });
const { APP, check, shot } = t;
const ARRAYS = "core.data-and-memory.arrays";
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
    await page.waitForTimeout(250);
  }
  return false;
};

await t.launch();
try {
  const page = await t.newPage();

  // ======================= part 1: the page, from seeded progress =======================
  await page.goto(`${APP}/#/stats`);
  await page.getByRole("heading", { name: "Learning stats" }).waitFor();
  check(/Learning stats/.test(await page.title()), "the page has its own title");
  check((await page.getByRole("link", { name: "Stats" }).count()) >= 1, "the nav has a Stats link");
  check(((await page.locator(".stat-tile", { hasText: "Lessons completed" }).textContent()) ?? "").includes("0"), "a new learner starts at 0 lessons");
  check((await page.locator(".heat li").count()) === 84 && (await page.locator(".heat li.future").count()) <= 6, "the activity view is 12 weeks of days");
  check((await page.getByText("Nothing stands out yet").count()) === 1 && (await page.getByText("No compiler errors or sanitizer reports recorded yet").count()) === 1, "empty sections say so");

  // five warm-up cards, all wrong, so there is an accuracy to show
  await page.goto(`${APP}/#/warmup`);
  for (let i = 0; i < 5; i++) {
    await page.locator(".warm-prompt").waitFor();
    await page.getByRole("button", { name: /don.t know/ }).click();
    await page.getByRole("button", { name: /Next card|Finish/ }).click();
  }
  await until(page, (p) => Object.values(p?.activity ?? {}).some((d) => d.warmups >= 5));
  await editIdb(page, () => {
    const day = (n) => new Date(Date.now() - n * 86400000).toISOString();
    p.lessons["core.foundations.hello-world"] = { status: "complete", runsToFirstPass: 1, msToFirstPass: 120000, timeMs: 150000, firstCompleteAt: day(5) };
    p.lessons["core.data-and-memory.arrays"] = { status: "complete", runsToFirstPass: 6, msToFirstPass: 1200000, timeMs: 1500000, hintsUsed: 3, solutionRevealed: true, firstCompleteAt: day(3) };
    p.lessons["core.foundations.variables"] = { status: "complete", runsToFirstPass: 1, msToFirstPass: 60000, timeMs: 90000, firstCompleteAt: day(2) };
    p.errors = {
      "compile:std-stream-undeclared": { count: 7, lastAt: "2026-05-02T10:00:00.000Z" },
      "compile:other": { count: 2, lastAt: "2026-05-03T10:00:00.000Z" },
      "runtime:AddressSanitizer: heap-use-after-free": { count: 1, lastAt: "2026-05-04T10:00:00.000Z" },
    };
    const d = new Date();
    const key = `${d.getFullYear()}-${String(d.getMonth() + 1).padStart(2, "0")}-${String(d.getDate()).padStart(2, "0")}`;
    p.activity[key] = { ...(p.activity[key] ?? {}), runs: 12, warmups: 5, resolves: 0, lessons: 0, ms: 600000 };
  });
  await page.goto(`${APP}/#/stats`);
  await page.reload(); // the app reads the saved progress at start
  await page.getByRole("heading", { name: "Learning stats" }).waitFor();
  const tile = (name) => page.locator(".stat-tile", { hasText: name }).textContent().then((s) => (s ?? "").replace(/\s+/g, " ").trim());
  check((await tile("Lessons completed")).includes("3"), "3 lessons completed");
  check(/67%.*of 3 measured/.test(await tile("Passed on the first try")), `first-try rate 67% of 3 (${await tile("Passed on the first try")})`);
  check(/1 \(median\)/.test(await tile("Typical test runs")), "median test runs 1");
  check(/Time on lessons\s*(?:\d+ h )?\d+ min/.test(await tile("Time on lessons")) || /Time on lessons\s*\d+ h/.test(await tile("Time on lessons")), `total time shown (${await tile("Time on lessons")})`);
  check(/0%/.test(await tile("Warm-up accuracy")), "warm-up accuracy reflects the five missed cards");

  const today = await page.locator(".heat li:not(.future)").last().locator(".sr").textContent();
  check(/12 runs, 5 cards/.test(today ?? ""), `today's square is described in words (${today})`);
  check((await page.locator(".heat li.lv3, .heat li.lv4").count()) >= 1, "a busy day is drawn darker");
  const week = (await page.locator(".stat-table").first().textContent()) ?? "";
  check(/Time on lessons10 min/.test(week.replace(/\s+/g, " ").replace(" 10 min", "10 min")) || /10 min/.test(week), "this week's time is 10 min");

  const effort = page.locator("section", { hasText: "took the most effort" }).locator("tbody tr");
  check((await effort.count()) === 1 && /Arrays/.test((await effort.first().textContent()) ?? ""), "the effort table lists the hard lesson (Arrays) and leaves out the easy ones");
  check(/6/.test((await effort.first().textContent()) ?? "") && /20 min/.test((await effort.first().textContent()) ?? "") && /looked/.test((await effort.first().textContent()) ?? ""), "with its 6 runs, 20 minutes, and the solution looked at");

  const errs = page.locator("section", { hasText: "Problems you hit most" }).locator(".stat-list li");
  check((await errs.count()) === 3, "three kinds of problem are listed");
  check(/^7×/.test((await errs.nth(0).textContent()) ?? "") && /std/.test((await errs.nth(0).textContent()) ?? "") && (await errs.nth(0).getByRole("link", { name: "Learn more" }).count()) === 1, "the most frequent comes first, in plain words, with a Learn more link");
  check(/2×.*Other compiler errors/.test((await errs.nth(1).textContent()) ?? ""), "unexplained errors are grouped as 'Other'");
  check(/Used memory after it was freed/.test((await errs.nth(2).textContent()) ?? ""), "a sanitizer report gets its plain title");
  check((await page.locator("section", { hasText: "Warm-up accuracy by topic" }).locator("meter").count()) >= 1, "warm-up accuracy is shown per topic");
  await shot(page, "stats-1");
  for (const th of ["dark", "light", "contrast"]) {
    await page.evaluate((x) => (document.documentElement.dataset.theme = x), th);
    const bad = (await new AxeBuilder({ page }).withTags(["wcag2a", "wcag2aa", "wcag21aa"]).analyze()).violations.filter((v) => v.impact === "serious" || v.impact === "critical");
    for (const v of bad) console.log(`   ${v.id}: ${v.nodes.slice(0, 2).map((n) => n.target.join(" ")).join(" | ")}`);
    check(bad.length === 0, `the stats page has no serious accessibility violations (${th})`);
  }
  await page.evaluate(() => (document.documentElement.dataset.theme = "dark"));
  await page.close();

  // ======================= part 2: real recording (needs Docker) =======================
  const live = await t.newPage();
  await t.open(live, ARRAYS);
  await t.setFile(live, "main.cpp", "int main() { return 0 }\n"); // a missing semicolon
  await t.runTests(live, /did not compile/);
  check(await until(live, (p) => Object.keys(p?.errors ?? {}).some((k) => k.startsWith("compile:"))), "a compile error is recorded in the tally");
  const sol = await t.solutionFiles(DIR);
  for (const [name, text] of Object.entries(sol)) await t.setFile(live, name, text);
  await t.runTests(live, /All tests passed/);
  check(await until(live, (p) => p?.lessons?.[ARRAYS]?.status === "complete" && p.lessons[ARRAYS].msToFirstPass > 0), "the time to the first pass is recorded");
  const p = await readIdb(live);
  const l = p.lessons[ARRAYS];
  check(l.timeMs >= l.msToFirstPass && l.timeMs > 1000, `time on the lesson is recorded (${Math.round(l.timeMs / 1000)} s)`);
  check(l.runsToFirstPass === 2, `it took 2 test runs (${l.runsToFirstPass})`);
  const act = Object.values(p.activity).reduce((n, d) => ({ runs: n.runs + d.runs, ms: n.ms + d.ms }), { runs: 0, ms: 0 });
  check(act.runs >= 2 && act.ms > 1000, `the day's activity counts the runs and the time (${act.runs} runs, ${Math.round(act.ms / 1000)} s)`);
  await live.goto(`${APP}/#/stats`);
  await live.getByRole("heading", { name: "Problems you hit most" }).waitFor();
  check((await live.locator("section", { hasText: "Problems you hit most" }).locator(".stat-list li").count()) >= 1, "the stats page lists the error just made");
  check(/50%/.test((await live.locator(".stat-tile", { hasText: "Passed on the first try" }).textContent()) ?? "") || /0%/.test((await live.locator(".stat-tile", { hasText: "Passed on the first try" }).textContent()) ?? ""), "the first-try rate reflects needing two runs");
  await shot(live, "stats-2");
  await live.close();
} catch (e) {
  t.crashed(e);
}
await t.finish();
