// UI checks for the domain challenge tracks (Games, Finance, Systems): the home page shows them apart from the course, recommends but does not lock,
// reference solutions pass through the real UI (including a thread/TSan challenge), and finishing a domain at the Works tier earns its badge.
//   usage: node domains.mjs        env: APP_URL (default http://localhost:5173), SHOTS (screenshot dir)
import { createSuite } from "./lib.mjs";

const t = createSuite({ track: "domains/" });
const { APP, check, shot, open, runTests, solve, newPage } = t;
const GAMES = [
  ["domain.games.ecs", "games/01-entity-component-system", "ECS"],
  ["domain.games.collision", "games/02-collision-detection", "Collision"],
  ["domain.games.astar", "games/03-astar-pathfinding", "A*"],
  ["domain.games.object-pool", "games/04-object-pool", "Object pool"],
  ["domain.games.game-loop", "games/05-fixed-timestep-loop", "Game loop"],
  ["domain.games.inventory", "games/06-inventory-and-items", "Inventory"],
];
const SOME_FINANCE = [
  ["domain.finance.money", "finance/01-fixed-point-money", "Fixed-point money"],
  ["domain.finance.event-queue", "finance/06-event-queue", "SPSC event queue (threads + TSan)"],
];
const SOME_SYSTEMS = [
  ["domain.systems.lru-cache", "systems/01-lru-cache", "LRU cache"],
  ["domain.systems.timer-wheel", "systems/07-timer-wheel", "Timer wheel"],
];

await t.launch();

try {
  const page = await newPage();
  await page.goto(`${APP}/#/`);
  await page.getByRole("heading", { name: "Domain challenge tracks" }).waitFor();
  check(true, "the home page has a 'Domain challenge tracks' section");

  const domain = (title) => page.locator("section.module").filter({ has: page.locator("h2", { hasText: title }) });
  const games = domain("Games"), finance = domain("Finance and Trading"), systems = domain("Systems and Infrastructure");
  check((await games.locator(".lesson-list a").count()) === 6, "Games lists six challenges");
  check((await finance.locator(".lesson-list a").count()) === 6, "Finance lists six challenges");
  check((await systems.locator(".lesson-list a").count()) === 7, "Systems lists seven challenges");
  check(/Recommended first/.test((await games.textContent()) ?? ""), "each domain recommends the course modules to do first");

  // the course sections come before the domain sections, and the domains are not mixed into the course
  const order = await page.locator("section.module h2").allTextContents();
  const firstDomain = order.findIndex((h) => /Games/.test(h));
  const lastCore = order.findIndex((h) => /Hero Projects/.test(h));
  check(lastCore >= 0 && firstDomain > lastCore, "the domain tracks come after Module 11");

  // recommend only: a challenge opens without finishing anything first
  await open(page, "domain.games.ecs");
  const starter = await runTests(page, /did not compile|tests passed|failed|time limit/);
  check(!/All tests passed/.test(starter), `the starter of the ECS challenge does not pass (${starter.slice(0, 80)})`);
  await shot(page, "domains-ecs-starter");

  // Games: all six at the Works tier earn the Game Dev badge and complete the track
  for (const [id, dir, label] of GAMES.slice(0, 5)) await solve(page, id, dir, label);
  await page.goto(`${APP}/#/profile`);
  check((await page.locator(".badge-card.earned", { hasText: "Game Dev" }).count()) === 0, "five of six Games challenges earn no Game Dev badge yet");
  await solve(page, ...GAMES[5]);
  await page.goto(`${APP}/#/profile`);
  check((await page.locator(".badge-card.earned", { hasText: "Game Dev" }).count()) === 1, "the sixth Games challenge earns the Game Dev badge");
  await shot(page, "domains-profile");
  await page.goto(`${APP}/#/`);
  const gamesHead = (await games.locator("h2").textContent()) ?? "";
  check(/module complete/.test(gamesHead), `the Games track shows as complete (${gamesHead.trim()})`);

  // Finance and Systems: a sample through the real UI (the full set is validated by validate-content in the same Docker runner)
  for (const [id, dir, label] of SOME_FINANCE) await solve(page, id, dir, label);
  for (const [id, dir, label] of SOME_SYSTEMS) await solve(page, id, dir, label);
  await page.goto(`${APP}/#/`);
  check(!/module complete/.test((await finance.locator("h2").textContent()) ?? ""), "two of six Finance challenges do not complete the track");
  check(!/module complete/.test((await systems.locator("h2").textContent()) ?? ""), "two of seven Systems challenges do not complete the track");
  await page.goto(`${APP}/#/profile`);
  check((await page.locator(".badge-card.earned", { hasText: "Quant" }).count()) === 0 && (await page.locator(".badge-card.earned", { hasText: "Systems Hacker" }).count()) === 0,
    "no Quant or Systems Hacker badge before those tracks are finished");
  check((await page.locator("details.soon .badge-card", { hasText: "Embedded Engineer" }).count()) === 1, "the Embedded Engineer badge waits in 'coming soon'");
  await page.close();
} catch (e) {
  t.crashed(e);
}

await t.finish();
