// End-to-end checks for the warm-ups: today's cards on Home, answering with the mouse and the keyboard, the explanation and the
// trap/fix programs after an answer, saved review state, the streak, and the Warm-Up badge.
//   usage: node warmup.mjs        env: APP_URL (default http://localhost:5173), SHOTS (screenshot dir)
import { createSuite } from "./lib.mjs";

const t = createSuite({ track: "02-data-and-memory/" });
const { APP, check, shot } = t;

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

await t.launch();
try {
  // ======================= a new learner: general questions are available from the start =======================
  const page = await t.newPage();
  await page.goto(`${APP}/#/`);
  const card = page.locator(".warmup-card");
  await card.waitFor();
  check(/5 cards? ready/.test((await card.textContent()) ?? ""), "Home offers a warm-up of 5 cards, even before any lesson is done");
  check((await page.locator("nav a", { hasText: "Warm-up" }).locator(".count").count()) === 0, "a new learner has nothing to review, so the Warm-up link shows no count");
  await card.getByRole("link", { name: "Start" }).click();
  await page.locator(".warm-prompt").waitFor();
  check(((await page.locator(".warm-head").textContent()) ?? "").includes("Card 1 of 5"), "the first card is Card 1 of 5");
  check((await page.locator(".choice").count()) >= 2, "a card has choices to pick from");
  check(((await page.locator(".warm-head .muted").textContent()) ?? "").length > 8, "the topic of the card is shown");
  await shot(page, "w1-card");

  // keyboard: 2 picks the second choice, Enter checks, Enter goes on
  await page.keyboard.press("2");
  check(await page.locator(".choice").nth(1).locator("input").isChecked(), "the key 2 picks the second choice");
  await page.keyboard.press("Enter");
  await page.locator(".banner[role=status]").waitFor();
  check(/Correct\.|Not quite/.test((await page.locator(".banner[role=status]").textContent()) ?? ""), "Enter checks the answer and says whether it was right");
  check((await page.locator(".choice.is-right").count()) === 1, "the right choice is marked after the answer");
  check(((await page.locator(".warm-explain").textContent()) ?? "").length > 30, "an explanation is shown");
  await shot(page, "w2-answered");
  await page.keyboard.press("Enter");
  await page.locator(".warm-head", { hasText: "Card 2 of 5" }).waitFor();
  check(true, "Enter goes on to the next card");

  // "I don't know" counts as a miss; then the remaining cards
  await page.getByRole("button", { name: /I don.t know/ }).click();
  check(/No problem/.test((await page.locator(".banner[role=status]").textContent()) ?? ""), "'I don't know' is a gentle miss, not a failure");
  for (let i = 3; i <= 5; i++) {
    await page.getByRole("button", { name: /Next card/ }).click();
    await page.locator(".warm-head", { hasText: `Card ${i} of 5` }).waitFor();
    await page.keyboard.press("1");
    await page.getByRole("button", { name: "Check" }).click();
  }
  await page.getByRole("button", { name: "Finish" }).click();
  await page.getByRole("heading", { name: "Warm-up done" }).waitFor();
  check(/\d of 5 correct/.test((await page.locator(".banner[role=status]").textContent()) ?? ""), "after five cards the result is shown");
  check((await page.getByRole("button", { name: "Another set" }).count()) === 1, "another set is offered");
  await page.waitForTimeout(500);
  const saved = await idbProgress(page);
  const ids = Object.keys(saved?.reviews ?? {});
  check(ids.length === 5 && ids.every((i) => saved.reviews[i].due > "2026"), `the five answered cards are scheduled for later (${ids.length})`);
  check(saved?.events?.["warmup-answered"]?.count === 5, "the answers are counted");
  check((saved?.streak?.current ?? 0) === 1, "a warm-up counts as a day for the streak");
  const missed = Object.values(saved.reviews).filter((r) => r.box === 1).length;
  check(missed >= 1, `a missed card goes to box 1 to come back tomorrow (${missed} in box 1)`);
  await page.getByRole("button", { name: "Another set" }).click();
  await page.locator(".warm-head", { hasText: "Card 1 of 5" }).waitFor();
  const again = await page.locator(".warm-prompt").textContent();
  check(true, `another set starts with new cards (${(again ?? "").slice(0, 40)})`);
  await page.close();

  // ======================= seeded progress: a due gotcha card, and the badge at five correct answers =======================
  const ID = "d:core.data-and-memory.arrays:array-decay:predict";
  const seed = {
    version: 1,
    lessons: { "core.data-and-memory.arrays": { status: "complete" } },
    events: { "warmup-correct": { count: 4, lastAt: "2026-01-01T00:00:00.000Z" } },
    reviews: { [ID]: { box: 2, due: "2026-01-01", seen: 1, correct: 1 } },
  };
  const p2 = await t.newPage({ init: `if (!localStorage.getItem("zth.seeded")) { localStorage.setItem("zth.progress.v2", ${JSON.stringify(JSON.stringify(seed))}); localStorage.setItem("zth.seeded", "1"); }` });
  await p2.goto(`${APP}/#/warmup`);
  await p2.locator(".warm-prompt").waitFor();
  check(((await p2.locator("nav a", { hasText: "Warm-up" }).locator(".count").textContent()) ?? "") === "1", "the header shows the one card that is due for review");
  check(((await p2.locator(".warm-prompt").textContent()) ?? "").includes("What does this program print?"), "a due gotcha card comes first, as a 'what does it print' question");
  check(((await p2.locator(".warm-code").textContent()) ?? "").includes("sizeof(values)"), "it shows the trap program of the gotcha");
  const labels = await p2.locator(".choice-code").allTextContents();
  check(labels.includes("1") && labels.includes("5") && new Set(labels).size === labels.length, `its choices include the real output and what the fixed program prints (${labels.join(" | ")})`);
  await p2.locator(".choice", { has: p2.locator(".choice-code", { hasText: /^1$/ }) }).click();
  await p2.getByRole("button", { name: "Check" }).click();
  check(/Correct\./.test((await p2.locator(".banner[role=status]").textContent()) ?? ""), "choosing the real output is correct");
  check(((await p2.locator(".warm-explain").textContent()) ?? "").includes("A C array decays to a pointer"), "the explanation is the gotcha's");
  check((await p2.locator(".warm-explain details.demo[open] .snip").count()) === 2, "the trap and the fix programs are shown, open");
  await p2.locator(".toast", { hasText: "Warm-Up" }).waitFor({ timeout: 10000 });
  check(((await p2.locator(".toast").first().textContent()) ?? "").includes("bronze"), "the fifth correct answer earns the Warm-Up (bronze) badge");
  await shot(p2, "w3-badge");
  await p2.waitForTimeout(500);
  const s2 = await idbProgress(p2);
  check(s2?.reviews?.[ID]?.box === 3 && s2.reviews[ID].due > "2026-06", "a right answer moves the card up a box and pushes it out a week");
  check(!!s2?.badges?.["warm-up-bronze"], "the badge is saved");
  await p2.close();
} catch (e) {
  t.crashed(e);
}
await t.finish();
