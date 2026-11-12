// Accessibility and display-preference checks: axe-core over the main pages in every theme, keyboard operation of the tabs,
// the skip link, page titles, and the Settings that change the look and the editor.
//   usage: node a11y.mjs        env: APP_URL (default http://localhost:5173), SHOTS (screenshot dir)
import AxeBuilder from "@axe-core/playwright";
import { createSuite } from "./lib.mjs";

const t = createSuite({ track: "01-foundations/" });
const { APP, check, shot } = t;
const LESSON = "core.data-and-memory.arrays";

/** serious or critical axe violations (Monaco renders its own markup, which is audited by Monaco, not here). */
async function violations(page, label) {
  const r = await new AxeBuilder({ page }).exclude(".monaco-editor").withTags(["wcag2a", "wcag2aa", "wcag21a", "wcag21aa", "wcag22aa"]).analyze();
  const bad = r.violations.filter((v) => v.impact === "serious" || v.impact === "critical");
  for (const v of bad) console.log(`   ${label}: ${v.id} (${v.impact}) ${v.help}: ${v.nodes.slice(0, 3).map((n) => `${n.target.join(" ")} ${n.any[0]?.data?.fgColor ?? ""}/${n.any[0]?.data?.bgColor ?? ""} ${n.any[0]?.data?.contrastRatio ?? ""}`).join(" | ")}`);
  check(bad.length === 0, `${label}: no serious or critical accessibility violations`);
}

const THEME_LABEL = { dark: /^Dark/, light: /^Light/, contrast: /^High contrast/ };

await t.launch();
try {
  const page = await t.newPage();
  const theme = () => page.evaluate(() => document.documentElement.dataset.theme);
  const fontSize = () => page.evaluate(() => document.documentElement.style.fontSize);

  // ---- defaults, title, skip link
  await page.goto(`${APP}/#/`);
  await page.getByLabel("Search lessons, concepts and gotchas").waitFor();
  check((await theme()) === "dark", "the default theme is dark");
  check(/Course map/.test(await page.title()), `the home page has a descriptive title (${await page.title()})`);
  await page.evaluate(() => document.activeElement instanceof HTMLElement && document.activeElement.blur());
  await page.keyboard.press("Tab");
  const act = await page.evaluate(() => document.activeElement?.outerHTML.slice(0, 80));
  check(await page.locator(".skip-link").evaluate((el) => el === document.activeElement), `the first Tab stop is the skip link (${act})`);
  await page.keyboard.press("Enter");
  check(await page.evaluate(() => document.activeElement?.tagName === "MAIN"), "the skip link moves focus to the main content");

  // ---- settings: theme, text size, reduced motion, persistence
  await page.goto(`${APP}/#/settings`);
  await page.getByRole("heading", { name: "Appearance" }).waitFor();
  check(/Settings/.test(await page.title()), "the Settings page has its own title");
  await page.getByRole("radio", { name: THEME_LABEL.light }).check();
  check((await theme()) === "light", "choosing Light applies the light theme at once");
  await page.getByRole("radio", { name: THEME_LABEL.contrast }).check();
  check((await theme()) === "contrast", "choosing High contrast applies it");
  await page.getByLabel(/Text size/).fill("130");
  check((await fontSize()) === "130%", "the text size slider scales the page");
  await page.getByLabel(/Reduce motion/).check();
  check((await page.evaluate(() => document.documentElement.dataset.motion)) === "reduce", "Reduce motion sets the motion switch");
  await page.reload();
  await page.getByRole("heading", { name: "Appearance" }).waitFor();
  check((await theme()) === "contrast" && (await fontSize()) === "130%", "the choices survive a reload");
  await page.emulateMedia({ colorScheme: "dark" });
  await page.getByRole("radio", { name: /^Match my system/ }).check();
  check((await theme()) === "dark", "'Match my system' resolves to dark when the system has no preference");
  await page.emulateMedia({ colorScheme: "light" });
  await page.waitForFunction(() => document.documentElement.dataset.theme === "light");
  check(true, "'Match my system' follows the system's light setting live");
  await page.emulateMedia({ colorScheme: "dark", contrast: "more" });
  await page.waitForFunction(() => document.documentElement.dataset.theme === "contrast");
  check(true, "'Match my system' follows the system's high-contrast setting");
  await page.emulateMedia({ colorScheme: null, contrast: null });
  await page.getByRole("button", { name: "Restore display defaults" }).click();
  check((await theme()) === "dark" && (await fontSize()) === "100%", "Restore display defaults goes back to dark at 100%");

  // ---- axe over every page, in every theme
  for (const th of ["dark", "light", "contrast"]) {
    await page.goto(`${APP}/#/settings`);
    await page.getByRole("radio", { name: THEME_LABEL[th] }).check();
    const pages = [
      ["home", "/", () => page.getByLabel("Search lessons, concepts and gotchas").waitFor()],
      ["settings", "/settings", () => page.getByRole("heading", { name: "Appearance" }).waitFor()],
      ["profile", "/profile", () => page.locator("main.profile").waitFor()],
      ["warm-up", "/warmup", () => page.locator("main.warmup").waitFor()],
    ];
    for (const [name, path, ready] of pages) {
      await page.goto(`${APP}/#${path}`);
      await ready();
      await violations(page, `${name} (${th})`);
    }
    await t.open(page, LESSON);
    await violations(page, `lesson (${th})`);
    if (th === "dark") {
      for (const name of [/^Gotchas/, "Hints", "Tutor"]) {
        await page.getByRole("tab", { name }).click();
        await violations(page, `lesson ${name} tab (${th})`);
      }
    }
    await shot(page, `a11y-${th}`);
  }

  // ---- keyboard operation of the tabs
  await t.open(page, LESSON);
  await page.locator("#left-tab-lesson").focus();
  await page.keyboard.press("ArrowRight");
  check((await page.locator("#left-tab-fast").getAttribute("aria-selected")) === "true" && (await page.locator("#left-tab-fast").evaluate((e) => e === document.activeElement)), "ArrowRight selects and focuses the next lesson tab");
  await page.keyboard.press("End");
  check((await page.locator("#left-tab-tutor").getAttribute("aria-selected")) === "true", "End jumps to the last tab");
  await page.keyboard.press("Home");
  check((await page.locator("#left-tab-lesson").getAttribute("aria-selected")) === "true", "Home jumps to the first tab");
  check((await page.locator("#left-tab-fast").getAttribute("tabindex")) === "-1" && (await page.locator("#left-tab-lesson").getAttribute("tabindex")) === "0", "only the selected tab is a tab stop");
  check((await page.locator("#left-panel").getAttribute("aria-labelledby")) === "left-tab-lesson", "the panel is labelled by its tab");
  check((await page.locator("main").count()) === 1 && (await page.title()).includes("·"), `the lesson page has one main landmark and a lesson title (${await page.title()})`);

  // ---- no horizontal scrolling at a phone-ish width on the content pages
  await page.setViewportSize({ width: 320, height: 700 });
  for (const path of ["/settings", "/warmup", "/profile"]) {
    await page.goto(`${APP}/#${path}`);
    await page.waitForTimeout(300);
    const over = await page.evaluate(() => document.documentElement.scrollWidth - document.documentElement.clientWidth);
    check(over <= 1, `${path} does not scroll sideways at 320px wide (${over}px over)`);
  }
  await page.setViewportSize({ width: 1400, height: 900 });

  // ---- editor preferences: theme, font, keybindings
  await page.goto(`${APP}/#/settings`);
  await page.getByRole("radio", { name: THEME_LABEL.light }).check();
  await page.getByLabel(/Font size/).fill("20");
  await t.open(page, LESSON);
  const cls = (await page.locator(".monaco-editor").first().getAttribute("class")) ?? "";
  check(!cls.includes("vs-dark") && !cls.includes("hc-black"), `the editor follows the light theme (${cls.split(" ").filter((c) => /^vs|^hc/.test(c)).join(" ")})`);
  const fs = await page.locator(".monaco-editor .view-lines").first().evaluate((e) => getComputedStyle(e).fontSize);
  check(fs === "20px", `the editor uses the chosen font size (${fs})`);
  check(!!(await page.locator(".monaco-editor [aria-label]").first().getAttribute("aria-label")), "the editor has an accessible name");

  await page.goto(`${APP}/#/settings`);
  await page.getByRole("radio", { name: THEME_LABEL.dark }).check();
  await page.getByRole("radio", { name: /^Vim/ }).check();
  await t.open(page, LESSON);
  await page.locator(".vim-status").waitFor({ timeout: 15000 });
  await page.locator(".monaco-editor .view-lines").first().click();
  await page.keyboard.press("Escape");
  await page.waitForFunction(() => /NORMAL/i.test(document.querySelector(".vim-status")?.textContent ?? ""), null, { timeout: 15000 });
  check(true, "Vim mode loads and shows NORMAL in the status line");
  await page.keyboard.press("i");
  await page.waitForFunction(() => /INSERT/i.test(document.querySelector(".vim-status")?.textContent ?? ""), null, { timeout: 5000 });
  check(true, "'i' switches to INSERT");

  await page.goto(`${APP}/#/settings`);
  await page.getByRole("radio", { name: /^Emacs/ }).check();
  await t.open(page, LESSON);
  await page.waitForTimeout(1500);
  check((await page.locator(".vim-status").count()) === 0, "Emacs mode shows no Vim status line");
  const before = await page.locator(".monaco-editor .view-lines").first().innerText();
  await page.locator(".monaco-editor .view-lines").first().click();
  await page.keyboard.press("Control+End");
  await page.keyboard.press("Control+a"); // Emacs: beginning of line
  await page.keyboard.press("Control+k"); // kill to end of line
  const after = await page.locator(".monaco-editor .view-lines").first().innerText();
  check(after !== before, "Emacs keys act on the text (Ctrl+A then Ctrl+K kills the line's text)");

  await page.goto(`${APP}/#/settings`);
  await page.getByRole("button", { name: "Restore display defaults" }).click();
  await page.close();
} catch (e) {
  t.crashed(e);
}
await t.finish();
