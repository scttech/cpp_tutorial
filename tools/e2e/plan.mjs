// End-to-end checks for Today's plan on Home: the steps (warm-up, re-solve, new lesson) appear and tick off, the choice stays put for the
// day, and a new day makes a new plan. Progress is seeded in IndexedDB, so no Docker is needed.
//   usage: node plan.mjs        env: APP_URL (default http://localhost:5173), SHOTS (screenshot dir)
import { createSuite } from "./lib.mjs";

const t = createSuite({ track: "01-foundations/" });
const { APP, check, shot } = t;

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
  for (let i = 0; i < 30; i++) {
    if (ok(await readIdb(page))) return true;
    await page.waitForTimeout(200);
  }
  return false;
};
const labels = (page) => page.locator(".plan-list li strong").allTextContents();

await t.launch();
try {
  const page = await t.newPage();

  // ---- a new learner: a warm-up and a first lesson
  await page.goto(`${APP}/#/`);
  const plan = page.locator(".plan-card");
  await plan.waitFor();
  check(JSON.stringify(await labels(page)) === JSON.stringify(["Warm-up", "New lesson"]), `a new learner's plan is a warm-up and a new lesson (${(await labels(page)).join(", ")})`);
  check(/0 of 2 done/.test((await plan.textContent()) ?? ""), "nothing is done yet (0 of 2)");
  check((await page.locator("main.home > section").first().evaluate((e) => e.classList.contains("plan-card"))), "the plan is the first card on Home");
  await until(page, (p) => !!p?.plan?.lessonId);
  const first = await readIdb(page);
  check(first.plan.date.length === 10 && !!first.plan.lessonId && !first.plan.resolveId, "the plan is saved with the day's lesson and no re-solve");
  await shot(page, "plan-1");

  // ---- do the warm-up
  await plan.getByRole("link", { name: "Warm-up" }).click();
  for (let i = 0; i < 5; i++) {
    await page.locator(".warm-prompt").waitFor();
    await page.getByRole("button", { name: /don.t know/ }).click();
    await page.getByRole("button", { name: /Next card|Finish/ }).click();
  }
  await page.goto(`${APP}/#/`);
  await plan.waitFor();
  const warm = plan.locator("li", { hasText: "Warm-up" });
  check(await warm.evaluate((e) => e.classList.contains("done")), "after five cards the warm-up step is ticked");
  check(/1 of 2 done/.test((await plan.textContent()) ?? "") && /day streak/.test((await plan.textContent()) ?? ""), "the header shows 1 of 2 done and the streak");
  check((await warm.getByRole("link").count()) === 0, "a finished step has no Start button");

  // ---- a lesson passed long ago becomes the re-solve step of a new plan
  await until(page, (p) => Object.values(p?.activity ?? {}).some((d) => d.warmups >= 5)); // saved a moment after it happens
  await editIdb(page, () => {
    p.lessons["core.foundations.hello-world"] = { status: "complete", firstCompleteAt: new Date(Date.now() - 20 * 86400000).toISOString() };
    p.plan = null;
  });
  await page.reload();
  await plan.waitFor();
  await page.locator(".plan-list li").nth(2).waitFor();
  check(JSON.stringify(await labels(page)) === JSON.stringify(["Warm-up", "Re-solve", "New lesson"]), `with a lesson due the plan is warm-up, re-solve, new lesson (${(await labels(page)).join(", ")})`);
  check(await plan.locator("li", { hasText: "Warm-up" }).evaluate((e) => e.classList.contains("done")), "the warm-up answered earlier today still counts");
  const resolveStep = plan.locator("li", { hasText: "Re-solve" });
  check(/Hello/i.test((await resolveStep.textContent()) ?? ""), "the re-solve step names the lesson");
  await shot(page, "plan-2");

  // ---- start the re-solve from the plan and give up (that finishes the step)
  await resolveStep.getByRole("link").click();
  await page.locator(".resolve-bar").waitFor();
  check(/resolve=1/.test(page.url()), "Start opens the lesson in re-solve mode");
  await page.getByRole("button", { name: "Give up" }).click();
  await page.locator(".resolve-bar [role=status]").waitFor();
  await until(page, (p) => p?.resolves?.["core.foundations.hello-world"]?.count === 1);
  await page.goto(`${APP}/#/`);
  await plan.waitFor();
  check(await plan.locator("li", { hasText: "Re-solve" }).evaluate((e) => e.classList.contains("done")), "after the re-solve the step is ticked (a miss still counts as doing it)");
  check(/2 of 3 done/.test((await plan.textContent()) ?? ""), "2 of 3 done");

  // ---- finish the new lesson: it stays in the plan, now done, and the plan is complete
  const lessonId = (await readIdb(page)).plan.lessonId;
  const lessonText = (await plan.locator("li", { hasText: "New lesson" }).textContent()) ?? "";
  await editIdb(page, () => {
    const id = p.plan.lessonId;
    p.lessons[id] = { status: "complete", firstCompleteAt: new Date().toISOString() };
  });
  await page.reload();
  await plan.waitFor();
  const done = plan.locator("li", { hasText: "New lesson" });
  const doneText = (await done.textContent()) ?? "";
  check((await done.evaluate((e) => e.classList.contains("done"))) && /completed/.test(doneText) && doneText.includes(/\) (.*?) ·/.exec(lessonText)?.[1] ?? "?"), `the finished lesson stays in the plan, ticked (${doneText.slice(0, 60)})`);
  check(/3 of 3 done/.test((await plan.textContent()) ?? "") && (await plan.locator(".banner.ok").count()) === 1, "3 of 3 done: the plan says it is complete");
  check((await readIdb(page)).plan.lessonId === lessonId, "the day's lesson did not change when the lesson was finished");
  await shot(page, "plan-3");

  // ---- a new day: the old plan is replaced
  await editIdb(page, () => {
    p.plan.date = "2000-01-01";
  });
  await page.reload();
  await plan.waitFor();
  await until(page, (p) => p?.plan?.date !== "2000-01-01");
  const next = await readIdb(page);
  check(next.plan.date !== "2000-01-01" && next.plan.lessonId !== lessonId, "on a new day the plan is chosen again, with the next lesson");
  await page.close();
} catch (e) {
  t.crashed(e);
}
await t.finish();
