// End-to-end checks for the weak-spots panel: misses make a topic weak, the panel on Home, focused practice, clearing after N right
// answers in a row, and the setting that changes N.
//   usage: node weakspots.mjs        env: APP_URL (default http://localhost:5173), SHOTS (screenshot dir)
import AxeBuilder from "@axe-core/playwright";
import { createSuite } from "./lib.mjs";

const t = createSuite({ track: "02-data-and-memory/" });
const { APP, check, shot } = t;

await t.launch();
try {
  const page = await t.newPage();

  // ---- a new learner has no weak spots
  await page.goto(`${APP}/#/`);
  await page.locator(".warmup-card").waitFor();
  check((await page.locator(".weak-card").count()) === 0, "a new learner has no weak-spots panel");

  // ---- miss a whole set (the explanation shows the right answer, which we remember by prompt)
  await page.goto(`${APP}/#/warmup`);
  const right = new Map();
  for (let i = 0; i < 5; i++) {
    await page.locator(".warm-prompt").waitFor();
    const prompt = ((await page.locator(".warm-prompt").textContent()) ?? "").trim();
    await page.getByRole("button", { name: /don.t know/ }).click();
    await page.locator(".choice.is-right").waitFor();
    right.set(prompt, await page.locator(".choice").evaluateAll((els) => els.findIndex((e) => e.classList.contains("is-right"))));
    await page.getByRole("button", { name: /Next card|Finish/ }).click();
  }
  check(right.size === 5, `five cards were answered with "I don't know" (${right.size} distinct)`);

  // ---- the panel
  await page.goto(`${APP}/#/`);
  const panel = page.locator(".weak-card");
  await panel.waitFor();
  const items = await panel.locator(".weak-list li").count();
  check(items >= 1, `Home shows a weak-spots panel with ${items} item(s)`);
  const firstTitle = ((await panel.locator(".weak-list li strong").first().textContent()) ?? "").trim();
  const reasons = (await panel.locator(".weak-list li .muted").first().textContent()) ?? "";
  check(/missed \d+ time/.test(reasons), `each weak spot says why (${reasons.trim()})`);
  check(/0\/5/.test((await panel.locator(".weak-list li").first().textContent()) ?? ""), "progress toward clearing starts at 0/5");
  check((await page.locator("main.home > section").evaluateAll((els) => els.findIndex((e) => e.classList.contains("weak-card")) < els.findIndex((e) => e.classList.contains("module")) || !els.some((e) => e.classList.contains("module")))), "the panel sits above the modules");
  await shot(page, "weak-1-panel");
  for (const th of ["dark", "light"]) {
    await page.evaluate((x) => (document.documentElement.dataset.theme = x), th);
    const bad = (await new AxeBuilder({ page }).withTags(["wcag2a", "wcag2aa", "wcag21aa"]).analyze()).violations.filter((v) => v.impact === "serious" || v.impact === "critical");
    for (const v of bad) console.log(`   ${v.id}: ${v.nodes.slice(0, 2).map((n) => n.target.join(" ")).join(" | ")}`);
    check(bad.length === 0, `the panel has no serious accessibility violations (${th})`);
  }
  await page.evaluate(() => (document.documentElement.dataset.theme = "dark"));

  // ---- focused practice on one weak spot
  await panel.getByRole("link", { name: `Practice ${firstTitle}` }).click();
  await page.locator(".warm-prompt").waitFor();
  check(/Weak-spot practice/.test((await page.locator(".warm-head h1").textContent()) ?? ""), "practice opens as a Weak-spot practice set");
  check(((await page.locator(".warm-head .muted").textContent()) ?? "").includes(firstTitle), `its cards are from the chosen topic (${firstTitle})`);
  const prompt = ((await page.locator(".warm-prompt").textContent()) ?? "").trim();
  check(right.has(prompt), "the first card is one that was missed before");
  await page.locator(".choice").nth(right.get(prompt)).click();
  await page.getByRole("button", { name: "Check" }).click();
  check(/Correct\./.test((await page.locator(".banner[role=status]").textContent()) ?? ""), "answering it right is marked correct");

  // ---- one right answer is not enough at the default of 5
  await page.goto(`${APP}/#/`);
  await panel.waitFor();
  const row = panel.locator(".weak-list li", { hasText: firstTitle });
  check(/1\/5/.test((await row.textContent()) ?? ""), "after one right answer it shows 1/5 and is still weak");

  // ---- the setting: clear after 1
  await page.goto(`${APP}/#/settings`);
  await page.getByLabel(/Clear a weak spot after/).fill("1");
  check(/after 1 right answer in a row/.test((await page.locator("section", { hasText: "Weak spots" }).first().textContent()) ?? ""), "the setting reads 'after 1 right answer in a row'");
  await page.goto(`${APP}/#/`);
  await page.locator("main.home").waitFor();
  check((await page.locator(".weak-card .weak-list li", { hasText: firstTitle }).count()) === 0, "with the setting at 1, the topic with one right answer in a row is cleared");
  const others = await page.locator(".weak-card .weak-list li").count();
  check(others === items - 1, `the other weak spots remain (${others} of ${items - 1})`);

  // ---- a miss brings it back
  await page.goto(`${APP}/#/warmup?weak=${encodeURIComponent("all")}`);
  await page.locator(".warm-prompt").waitFor();
  await page.getByRole("button", { name: /don.t know/ }).click();
  await page.goto(`${APP}/#/`);
  await page.locator(".weak-card").waitFor();
  check((await page.locator(".weak-card").count()) === 1, "after another miss the panel is still there");

  // ---- survives a reload (saved with progress)
  await page.reload();
  await page.locator(".weak-card").waitFor();
  check(true, "the weak spots survive a reload");
  await page.close();
} catch (e) {
  t.crashed(e);
}
await t.finish();
