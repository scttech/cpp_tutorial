// End-to-end checks for the Gotchas tab: a lesson's gotcha ids, explained from its gotchas.yaml, and the graceful
// fallback (readable titles) for a lesson whose explanations are not written yet.
//   usage: node gotchas.mjs        env: APP_URL (default http://localhost:5173), SHOTS (screenshot dir)
import { createSuite } from "./lib.mjs";

const t = createSuite({ track: "02-data-and-memory/" });
const { check, shot } = t;
const ARRAYS = "core.data-and-memory.arrays";

await t.launch();
try {
  const page = await t.newPage();
  await t.open(page, ARRAYS);
  const tab = page.locator(".left .tab", { hasText: "Gotchas" });
  check((await tab.textContent())?.includes("(4)"), "the lesson page has a Gotchas tab that counts the lesson's four gotchas");
  const order = (await page.locator(".left .tab").allTextContents()).map((s) => s.replace(/\s*\(\d+\)/, "").trim());
  check(order.join(",").startsWith("Lesson,Fast summary,Gotchas,Hints"), `tabs run Lesson, Fast summary, Gotchas, Hints (${order.join(", ")})`);
  await tab.click();
  check((await page.locator(".gotchas li").count()) === 4, "four gotchas are listed");
  const titles = await page.locator(".gotchas .g-title").allTextContents();
  check(titles[0] === "A C array decays to a pointer" && titles[1] === "Indexing is not bounds-checked", `they follow lesson.yaml's order (${titles[0]} / ${titles[1]})`);
  const sizeofTitle = page.locator(".gotchas .g-title").nth(2);
  check(((await sizeofTitle.textContent()) ?? "").startsWith("sizeof on") && (await sizeofTitle.locator("code").count()) === 1, "backticks in a title render as code, not as literal backticks");
  check(((await page.locator(".gotchas li").first().textContent()) ?? "").includes("pointer to the first element"), "each one has its explanation");
  check((await page.locator(".gotchas li").first().locator(".g-text code").count()) >= 1, "inline code in an explanation is rendered as code");
  await shot(page, "g1-gotchas");

  // ---- runnable demos ----
  const decoy = page.locator(".gotchas li").first();
  check((await decoy.locator("details.demo").count()) === 1 && !(await decoy.locator("details.demo").evaluate((d) => d.open)), "a gotcha with a demo has a collapsed 'Try it' block");
  await decoy.locator("details.demo summary").click();
  check((await decoy.locator(".snip").count()) === 2, "it holds two programs: the trap and the fix");
  const trap = decoy.locator(".snip.bad");
  const fix = decoy.locator(".snip.fix");
  check(((await trap.locator("textarea").inputValue()) ?? "").includes("sizeof(values)"), "the trap's code is shown in an editable box");
  await trap.getByRole("button", { name: /Run/ }).click();
  await trap.locator(".snip-out").first().waitFor({ timeout: 60000 });
  const trapOut = await trap.locator(".snip-out:not(.warn):not(.err)").last().innerText();
  check(trapOut.trim() === "1", `running the trap shows its output (${trapOut.trim()})`);
  await fix.getByRole("button", { name: /Run/ }).click();
  await fix.locator(".snip-out").first().waitFor({ timeout: 60000 });
  check((await fix.locator(".snip-out").first().innerText()).trim() === "5", "running the fix prints 5");

  // edit, run, reset
  await fix.locator("textarea").fill('#include <iostream>' + String.fromCharCode(10) + 'int main() { std::cout << 7; }' + String.fromCharCode(10));
  await fix.getByRole("button", { name: /Run/ }).click();
  await page.waitForFunction(() => [...document.querySelectorAll(".snip.fix .snip-out")].some((e) => e.textContent?.trim() === "7"), null, { timeout: 60000 });
  check(true, "the code can be edited and run");
  await fix.getByRole("button", { name: "Reset" }).click();
  check(((await fix.locator("textarea").inputValue()) ?? "").includes("std::array"), "Reset restores the original code");

  // a compile error is shown, not swallowed
  await trap.locator("textarea").fill("int main( {");
  await trap.getByRole("button", { name: /Run/ }).click();
  await trap.locator(".snip-out.err").first().waitFor({ timeout: 60000 });
  check(((await trap.locator(".snip-out.err").first().innerText()) ?? "").includes("error"), "a compile error is shown");

  // a sanitizer demo (the full version): the report is shown
  const bounds = page.locator(".gotchas li").nth(1);
  await bounds.locator("details.demo summary").click();
  await bounds.locator(".snip.bad").getByRole("button", { name: /Run/ }).click();
  await bounds.locator(".snip.bad .snip-out.err").first().waitFor({ timeout: 60000 });
  check(((await bounds.locator(".snip.bad .snip-out.err").first().innerText()) ?? "").includes("out of bounds"), "a sanitizer demo shows the runtime report");
  await shot(page, "g2-demo");

  // every lesson is explained now: a domain lesson shows all its gotchas with titles and explanations (the fallback for a missing
  // explanation is covered by the unit tests of gotchaEntries)
  await t.open(page, "domain.games.ecs");
  await page.locator(".left .tab", { hasText: "Gotchas" }).click();
  const n = await page.locator(".gotchas li").count();
  const first = (await page.locator(".gotchas .g-title").first().textContent()) ?? "";
  check(n === 12 && first === "A handle must notice that its entity is gone", `a domain lesson lists its ${n} gotchas (${first})`);
  check((await page.locator(".gotchas .g-text").count()) === 12, "…every one with its explanation");
  await page.close();
} catch (e) {
  t.crashed(e);
}
await t.finish();
