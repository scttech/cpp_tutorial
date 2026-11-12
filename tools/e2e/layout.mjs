// End-to-end checks for the lesson page layout: results beside the editor on a wide screen, stacked on a normal one, and the setting.
//   usage: node layout.mjs        env: APP_URL (default http://localhost:5173), SHOTS (screenshot dir)
import { createSuite } from "./lib.mjs";

const t = createSuite({ track: "02-data-and-memory/" });
const { APP, check, shot } = t;
const LESSON = "core.data-and-memory.arrays";

await t.launch();
try {
  const page = await t.newPage();
  const layout = () => page.evaluate(() => document.documentElement.dataset.layout);
  const direction = () => page.locator(".panes").evaluate((e) => getComputedStyle(e).flexDirection);
  const box = (sel) => page.locator(sel).first().boundingBox();

  // ---- 1400px: stacked
  await t.open(page, LESSON);
  check((await layout()) === "normal" && (await direction()) === "column", "at 1400px wide the results are stacked below the editor");
  const stackedEditor = await box(".editor-wrap");

  // ---- 2200px: side by side, nothing overflowing
  await page.setViewportSize({ width: 2200, height: 1100 });
  await page.waitForFunction(() => document.documentElement.dataset.layout === "wide");
  check((await direction()) === "row", "at 2200px wide the results sit beside the editor");
  const [lesson, editor, results] = [await box(".left"), await box(".editor-wrap"), await box(".results")];
  check(lesson.x < editor.x && editor.x < results.x && results.x >= editor.x + editor.width - 2, "three columns, left to right: lesson, editor, results");
  check(editor.height > stackedEditor.height + 100, `the editor is taller when the results move aside (${Math.round(stackedEditor.height)} to ${Math.round(editor.height)}px)`);
  check(results.width >= 420 && editor.width >= 500, `both columns have a usable width (editor ${Math.round(editor.width)}px, results ${Math.round(results.width)}px)`);
  check((await page.evaluate(() => document.documentElement.scrollWidth <= document.documentElement.clientWidth)), "no sideways scrolling");
  await shot(page, "layout-wide");

  // ---- the setting
  await page.goto(`${APP}/#/settings`);
  await page.getByRole("radio", { name: /^Always stacked/ }).check();
  await t.open(page, LESSON);
  check((await layout()) === "normal" && (await direction()) === "column", "'Always stacked' keeps the stacked layout on a wide screen");
  await page.goto(`${APP}/#/settings`);
  await page.getByRole("radio", { name: /^Always wide/ }).check();
  await page.setViewportSize({ width: 1400, height: 900 });
  await t.open(page, LESSON);
  check((await layout()) === "wide" && (await direction()) === "row", "'Always wide' uses the side-by-side layout at 1400px");
  await page.goto(`${APP}/#/settings`);
  await page.getByRole("radio", { name: /^Automatic/ }).check();
  await t.open(page, LESSON);
  check((await layout()) === "normal", "'Automatic' is back to following the screen width");

  // ---- tests still work in the wide layout: the results panel is usable
  await page.setViewportSize({ width: 2200, height: 1100 });
  await page.getByRole("tab", { name: "Console" }).click();
  check(await page.locator(".results .panel-body").isVisible(), "the results panel is usable in the wide layout");
  await page.close();
} catch (e) {
  t.crashed(e);
}
await t.finish();
