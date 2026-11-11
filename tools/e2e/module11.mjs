// UI checks for Module 11 (Hero Projects): each project is solved through the real UI with its reference solution (several files each).
// A module of six projects is complete only when all six are done; the "Zero to Hero" badge needs three, "Hero of Heroes" all six.
//   usage: node module11.mjs        env: APP_URL (default http://localhost:5173), SHOTS (screenshot dir)
import { createSuite } from "./lib.mjs";

const t = createSuite({ track: "11-hero-projects/" });
const { APP, check, shot, open, runTests, solve, newPage } = t;
const LESSONS = [
  ["core.heroes.json", "01-json-library", "JSON Library"],
  ["core.heroes.expression-compiler", "02-expression-compiler", "Expression Compiler"],
  ["core.heroes.mini-database", "03-mini-database", "Mini Database"],
  ["core.heroes.text-adventure", "04-text-adventure-engine", "Text Adventure Engine"],
  ["core.heroes.regex", "05-tiny-regex-engine", "Tiny Regex Engine"],
  ["core.heroes.allocator", "06-memory-allocator", "Memory Allocator"],
];

await t.launch();

try {
  const page = await newPage();
  await page.goto(`${APP}/#/`);
  const module11 = page.locator("section.module").filter({ has: page.locator("h2", { hasText: "Hero Projects" }) });
  await module11.waitFor();
  check((await module11.locator(".lesson-list a").count()) === LESSONS.length, "Module 11 lists the six hero projects (and no test-out)");

  // the starters are skeletons: they compile but do not finish the project
  await open(page, "core.heroes.json");
  const starter = await runTests(page, /did not compile|tests passed|failed|time limit/);
  check(!/All tests passed/.test(starter), `the starter of the JSON project does not finish it (${starter.slice(0, 90)})`);
  await shot(page, "m11-json-starter");

  // three projects: enough for Zero to Hero, but the module is not complete
  for (const [id, dir, label] of LESSONS.slice(0, 3)) await solve(page, id, dir, label);
  await page.goto(`${APP}/#/`);
  let head = (await module11.locator("h2").textContent()) ?? "";
  check(!/module complete/.test(head), `after three projects Module 11 is not complete (${head.trim()})`);

  for (const [id, dir, label] of LESSONS.slice(3)) await solve(page, id, dir, label);
  await shot(page, "m11-allocator-pass");

  await page.goto(`${APP}/#/`);
  head = (await module11.locator("h2").textContent()) ?? "";
  check(/module complete/.test(head), `after all six projects Module 11 is complete (${head.trim()})`);
  await shot(page, "m11-home");
  await page.close();
} catch (e) {
  t.crashed(e);
}

await t.finish();
