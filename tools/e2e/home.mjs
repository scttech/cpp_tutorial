// UI checks for the course map: collapsible modules, the "continue" card, and remembered open/closed choices.
// Progress is seeded straight into IndexedDB (the same record the app writes), so no lesson has to be run.
//   usage: node home.mjs        env: APP_URL (default http://localhost:5173), SHOTS (screenshot dir)
import { createSuite } from "./lib.mjs";

const t = createSuite();
const { APP, check, shot } = t;

const FIRST = "core.foundations.hello-world";
const SECOND = "Variables and fundamental types";

const lessonRecord = (status) => ({
  status,
  bestTiers: { works: status === "complete", robust: false, fast: false, clean: false },
  attempts: 1, testRuns: 1, failedRuns: 0, hintsUsed: 0, aiHelpUsed: 0, solutionRevealed: false, lastCode: {},
});

/** Replace the saved progress, then reload so the app reads it. */
async function seed(page, lessons) {
  const progress = {
    version: 1,
    lessons: Object.fromEntries(Object.entries(lessons).map(([id, s]) => [id, lessonRecord(s)])),
    streak: { current: 0, longest: 0, lastActiveDate: null, forgivenOn: null },
    badges: {}, events: {}, settings: { pace: "full" },
  };
  await page.evaluate(
    (value) =>
      new Promise((resolve, reject) => {
        const open = indexedDB.open("zth", 1);
        open.onerror = () => reject(open.error);
        open.onsuccess = () => {
          const tx = open.result.transaction("kv", "readwrite");
          tx.objectStore("kv").put(value, "progress");
          tx.oncomplete = () => resolve();
          tx.onerror = () => reject(tx.error);
        };
      }),
    progress,
  );
  await page.reload();
  await page.locator("section.module").first().waitFor();
}

const toggles = (page) => page.locator(".module-toggle");
const expanded = async (page) => (await toggles(page).evaluateAll((els) => els.map((e) => e.getAttribute("aria-expanded") === "true")));

await t.launch();
try {
  const page = await t.newPage();
  await page.goto(`${APP}/#/`);
  await page.locator("section.module").first().waitFor();

  // ---- a new learner: Module 1 is open, the others are closed, and the card says "Start here" ----
  let state = await expanded(page);
  check(state[0] === true && state.slice(1).every((x) => !x), `only the first module is open for a new learner (${state.join(",")})`);
  const card = page.locator(".continue-card");
  check(((await card.textContent()) ?? "").includes("Start here"), "the continue card says 'Start here' before any progress");
  check((await page.locator("section.module").nth(1).locator(".lesson-list").isVisible()) === false, "a closed module hides its lesson list");
  await shot(page, "home-new");

  // ---- progress: the card moves to the next lesson ----
  await page.goto(`${APP}/#/`);
  await seed(page, { [FIRST]: "complete" });
  const cardText = (await card.textContent()) ?? "";
  check(cardText.includes(SECOND) && cardText.includes("Up next"), `after finishing lesson 1 the card offers the next lesson (${cardText.trim()})`);
  await card.getByRole("link").click();
  await page.locator(".monaco-editor").first().waitFor({ timeout: 30000 });
  check(page.url().endsWith("/lesson/core.foundations.variables-and-types"), "the card's button opens that lesson");

  // ---- an in-progress lesson is picked up ----
  await page.goto(`${APP}/#/`);
  await seed(page, { [FIRST]: "in_progress" });
  check(((await card.textContent()) ?? "").includes("Pick up where you left off"), "an in-progress lesson is offered as 'Pick up where you left off'");

  // ---- toggling, keyboard access, and remembering the choice ----
  const second = toggles(page).nth(1);
  await second.focus();
  await page.keyboard.press("Enter");
  check((await second.getAttribute("aria-expanded")) === "true", "Enter on a module header opens it");
  check(await page.locator("section.module").nth(1).locator(".lesson-list").isVisible(), "the opened module shows its lessons");
  await toggles(page).first().click();
  state = await expanded(page);
  check(state[0] === false && state[1] === true, "modules open and close independently");
  await page.reload();
  await page.locator("section.module").first().waitFor();
  state = await expanded(page);
  check(state[0] === false && state[1] === true, "the choices survive a reload");

  // ---- no storage: the page still works ----
  const bare = await t.newPage({ init: "Object.defineProperty(window, 'localStorage', { get() { throw new Error('blocked'); } });" });
  await bare.goto(`${APP}/#/`);
  await bare.locator("section.module").first().waitFor();
  await toggles(bare).nth(2).click();
  check((await toggles(bare).nth(2).getAttribute("aria-expanded")) === "true", "toggling still works when localStorage is blocked");
} catch (e) {
  t.crashed(e);
}

await t.finish();
