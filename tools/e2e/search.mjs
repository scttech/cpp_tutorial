// End-to-end checks for the course search on the home page.
//   usage: node search.mjs        env: APP_URL (default http://localhost:5173), SHOTS (screenshot dir)
import { createSuite } from "./lib.mjs";

const t = createSuite({ track: "01-foundations/" });
const { APP, check, shot } = t;

await t.launch();
try {
  const page = await t.newPage();
  await page.goto(`${APP}/#/`);
  const box = page.getByLabel("Search lessons, concepts and gotchas");
  await box.waitFor();
  const modulesBefore = await page.locator("section.module").count();
  check(modulesBefore > 5 && (await page.locator(".results-list").count()) === 0, `with an empty box the modules are listed (${modulesBefore})`);

  // a word that only appears in a gotcha title
  await box.fill("slicing");
  const list = page.locator(".results-list");
  await list.waitFor();
  const links = await list.locator("li a").allTextContents();
  check(links.some((l) => /Inheritance basics/.test(l)), `"slicing" finds the inheritance lessons through their gotchas (${links.slice(0, 3).join(" | ")})`);
  const sub = (await list.locator("li", { hasText: "Inheritance basics" }).locator(".row-sub").first().textContent()) ?? "";
  check(/(gotcha|concept):/.test(sub) && /Inheritance and Polymorphism/.test(sub), `each result says which module it is in and where it matched (${sub.slice(0, 90)})`);
  check((await page.locator("section.module").count()) === 0, "while searching, the module list is replaced by the results");
  check(((await page.getByRole("status").filter({ hasText: /lesson/ }).first().textContent()) ?? "").includes(String(links.length)), "the number of results is announced");
  await shot(page, "s1-search");

  // all words must match, and a title match ranks first
  await box.fill("virtual functions");
  check((await list.locator("li a").first().textContent())?.includes("Virtual functions"), "a title match comes first");
  await box.fill("unique_ptr");
  check((await list.locator("li a").allTextContents()).some((l) => /unique_ptr/.test(l)), "punctuation in a query is handled (unique_ptr)");

  // nothing matches
  await box.fill("qqqqzzzz");
  check(((await page.getByRole("status").filter({ hasText: "No lessons match" }).count()) === 1) && (await list.locator("li").count()) === 0, "a query with no match says so");

  // filters
  await box.fill("");
  await page.getByLabel("Test-outs").check();
  const kinds = await list.locator("li .badge.kind").allTextContents();
  check(kinds.length > 5 && kinds.every((k) => k === "Test out"), `the Test-outs chip shows only test-outs (${kinds.length})`);
  await page.getByLabel("Test-outs").uncheck();
  await page.getByLabel("Challenges and debugging").check();
  const k2 = await list.locator("li .badge.kind").allTextContents();
  check(k2.length > 20 && k2.every((k) => ["Challenge", "Capstone", "Debug"].includes(k)), `the Challenges chip shows challenges, capstones and debug lessons (${k2.length})`);
  await box.fill("slicing");
  check((await list.locator("li").count()) <= 3, "a filter and a query combine");

  // clear
  await page.getByRole("button", { name: "Clear" }).click();
  check((await page.locator("section.module").count()) === modulesBefore && (await box.inputValue()) === "", "Clear restores the module list");
  await page.close();
} catch (e) {
  t.crashed(e);
}
await t.finish();
