import { describe, expect, it } from "vitest";
import { emptyLesson, emptyProgress } from "../state/model";
import type { LessonProgress, Progress } from "../state/model";
import { awardBadges, checkRule, evaluate, isAvailable, isModuleComplete, newlyEarned } from "./engine";
import type { BadgeDef, Catalog, Rule } from "./engine";

const T = (works: boolean, robust = false, fast = false, clean = false) => ({ works, robust, fast, clean });
const catalog: Catalog = {
  lessons: [
    { id: "m1.testout", module: "m1", type: "test-out", tiers: T(true, true) },
    { id: "m1.a", module: "m1", type: "lesson", tiers: T(true, true) },
    { id: "m1.b", module: "m1", type: "lesson", tiers: T(true, true, true) },
    { id: "m1.cap", module: "m1", type: "capstone", tiers: T(true, true) },
    { id: "m2.a", module: "m2", type: "lesson", tiers: T(true) },
  ],
};
const complete = (over: Partial<LessonProgress> = {}): LessonProgress => ({
  ...emptyLesson(),
  status: "complete",
  bestTiers: T(true, true, true),
  firstCompleteAt: "2026-05-05T12:00:00.000Z",
  ...over,
});
const withLessons = (lessons: Record<string, LessonProgress>): Progress => ({ ...emptyProgress(), lessons });
const ev = (rule: Rule, state: Progress) => evaluate(rule, state, catalog);

describe("lesson-complete / lesson-count", () => {
  it("lesson-complete is met only when that lesson is complete", () => {
    expect(ev({ type: "lesson-complete", lesson: "m1.a" }, emptyProgress()).met).toBe(false);
    expect(ev({ type: "lesson-complete", lesson: "m1.a" }, withLessons({ "m1.a": complete() })).met).toBe(true);
  });
  it("counts lessons matching a filter and reports evidence + progress", () => {
    const s = withLessons({ "m1.a": complete(), "m1.b": complete() });
    const r = ev({ type: "lesson-count", where: { status: "complete" }, atLeast: 3 }, s);
    expect(r).toMatchObject({ met: false, current: 2, target: 3 });
    expect(r.lessons.sort()).toEqual(["m1.a", "m1.b"]);
  });
  it("filters by type, module, hints and revealed solution", () => {
    const s = withLessons({
      "m1.a": complete({ hintsUsed: 2 }),
      "m1.b": complete(),
      "m1.cap": complete({ solutionRevealed: true }),
      "m2.a": complete(),
    });
    const where = { status: "complete" as const, hintsUsed: 0, aiHelpUsed: 0, solutionRevealed: false };
    expect(ev({ type: "lesson-count", where, atLeast: 1 }, s).lessons.sort()).toEqual(["m1.b", "m2.a"]);
    expect(ev({ type: "lesson-count", where: { ...where, module: "m1" }, atLeast: 1 }, s).lessons).toEqual(["m1.b"]);
    expect(ev({ type: "lesson-count", where: { type: ["capstone"], status: "complete" }, atLeast: 1 }, s).lessons).toEqual(["m1.cap"]);
  });
  it("allTiers requires every tier declared AND earned (so only 4-tier lessons can be flawless)", () => {
    const four: Catalog = { lessons: [{ id: "x", module: "m", type: "challenge", tiers: T(true, true, true, true) }, ...catalog.lessons] };
    const s = withLessons({ x: complete({ bestTiers: T(true, true, true, true) }), "m1.b": complete() });
    expect(evaluate({ type: "lesson-count", where: { allTiers: true }, atLeast: 1 }, s, four).lessons).toEqual(["x"]);
    expect(evaluate({ type: "lesson-count", where: { allTiers: true }, atLeast: 1 }, s, catalog).met).toBe(false);
  });
});

describe("module-complete", () => {
  it("is complete when every non-test-out lesson is complete", () => {
    const s = withLessons({ "m1.a": complete(), "m1.b": complete(), "m1.cap": complete() });
    expect(isModuleComplete(s, catalog, "m1")).toBe(true);
  });
  it("is complete when the test-out is passed, even if lessons are not", () => {
    expect(isModuleComplete(withLessons({ "m1.testout": complete() }), catalog, "m1")).toBe(true);
  });
  it("is not complete with one lesson missing, and unknown modules are never complete", () => {
    expect(isModuleComplete(withLessons({ "m1.a": complete(), "m1.b": complete() }), catalog, "m1")).toBe(false);
    expect(isModuleComplete(emptyProgress(), catalog, "nope")).toBe(false);
  });
  describe("a module that is still being written (inProgress)", () => {
    const wip: Catalog = { ...catalog, inProgressModules: ["m1"] };
    it("is NOT completed by finishing the lessons that exist so far", () => {
      const s = withLessons({ "m1.a": complete(), "m1.b": complete(), "m1.cap": complete() });
      expect(isModuleComplete(s, wip, "m1")).toBe(false);
    });
    it("can still be completed by the test-out", () => {
      expect(isModuleComplete(withLessons({ "m1.testout": complete() }), wip, "m1")).toBe(true);
    });
    it("only affects the modules listed", () => {
      expect(isModuleComplete(withLessons({ "m2.a": complete() }), wip, "m2")).toBe(true);
    });
    it("its module-complete badge is 'coming soon' (not available) until the module is finished", () => {
      expect(isAvailable({ type: "module-complete", module: "m1" }, wip)).toBe(false);
      expect(isAvailable({ type: "module-complete", module: "m1" }, catalog)).toBe(true);
      expect(isAvailable({ type: "all", rules: [{ type: "module-complete", module: "m1" }] }, wip)).toBe(false);
    });
  });
});

describe("tier-complete, first-try, persistence", () => {
  it("counts lessons that earned a tier (and only those that declare it)", () => {
    const s = withLessons({ "m1.a": complete(), "m1.b": complete(), "m2.a": complete() });
    expect(ev({ type: "tier-complete", tier: "works", atLeast: 3 }, s).met).toBe(true);
    expect(ev({ type: "tier-complete", tier: "fast", atLeast: 1 }, s).lessons).toEqual(["m1.b"]); // m2.a does not declare fast
  });
  it("first-try needs a lesson completed on its first test run", () => {
    expect(ev({ type: "first-try", atLeast: 1 }, withLessons({ "m1.a": complete({ runsToFirstPass: 2 }) })).met).toBe(false);
    expect(ev({ type: "first-try", atLeast: 1 }, withLessons({ "m1.a": complete({ runsToFirstPass: 1 }) })).met).toBe(true);
  });
  it("persistence needs 15+ failed runs and no revealed solution", () => {
    const rule: Rule = { type: "persistence", failedRunsAtLeast: 15 };
    expect(ev(rule, withLessons({ "m1.a": complete({ failedRuns: 14 }) })).met).toBe(false);
    expect(ev(rule, withLessons({ "m1.a": complete({ failedRuns: 15, solutionRevealed: true }) })).met).toBe(false);
    expect(ev(rule, withLessons({ "m1.a": complete({ failedRuns: 15 }) })).met).toBe(true);
  });
});

describe("streak-days, custom-event, same-day-count, all", () => {
  it("streak-days uses the longest streak", () => {
    const s = { ...emptyProgress(), streak: { current: 1, longest: 7, lastActiveDate: "x", forgivenOn: null } };
    expect(ev({ type: "streak-days", atLeast: 7 }, s).met).toBe(true);
    expect(ev({ type: "streak-days", atLeast: 8 }, s).met).toBe(false);
  });
  it("custom-event counts recorded events", () => {
    const s = { ...emptyProgress(), events: { segfault: { count: 2, lastAt: "" } } };
    expect(ev({ type: "custom-event", event: "segfault", atLeast: 1 }, s).met).toBe(true);
    expect(ev({ type: "custom-event", event: "timeout", atLeast: 1 }, s).met).toBe(false);
  });
  it("same-day-count groups completions by local day", () => {
    const day = (iso: string) => complete({ firstCompleteAt: iso });
    const cat: Catalog = { lessons: ["a", "b", "c"].map((id) => ({ id, module: "m", type: "test-out" as const, tiers: T(true) })) };
    const rule: Rule = { type: "same-day-count", where: { type: ["test-out"] }, atLeast: 3 };
    const sameDay = withLessons({ a: day("2026-05-05T10:00:00"), b: day("2026-05-05T11:00:00"), c: day("2026-05-05T12:00:00") });
    const spread = withLessons({ a: day("2026-05-05T10:00:00"), b: day("2026-05-06T11:00:00"), c: day("2026-05-07T12:00:00") });
    expect(evaluate(rule, sameDay, cat).met).toBe(true);
    expect(evaluate(rule, spread, cat).met).toBe(false);
  });
  it("all requires every sub-rule", () => {
    const rule: Rule = { type: "all", rules: [{ type: "module-complete", module: "m1" }, { type: "lesson-complete", lesson: "m2.a" }] };
    const m1 = { "m1.a": complete(), "m1.b": complete(), "m1.cap": complete() };
    expect(ev(rule, withLessons(m1)).met).toBe(false);
    expect(ev(rule, withLessons({ ...m1, "m2.a": complete() })).met).toBe(true);
  });
});

describe("newlyEarned / awardBadges / isAvailable", () => {
  const defs: BadgeDef[] = [
    { id: "first", name: "First", category: "progress", icon: "x", description: "", rule: { type: "lesson-count", where: { status: "complete" }, atLeast: 1 } },
    { id: "m2", name: "M2", category: "progress", icon: "x", description: "", rule: { type: "module-complete", module: "m2" } },
  ];
  it("reports only newly met, not-yet-earned badges", () => {
    const s = withLessons({ "m1.a": complete() });
    expect(newlyEarned(defs, s, catalog).map((e) => e.id)).toEqual(["first"]);
    const awarded = awardBadges(s, newlyEarned(defs, s, catalog), "2026-05-05T00:00:00Z");
    expect(awarded.badges.first).toMatchObject({ earnedAt: "2026-05-05T00:00:00Z", evidence: { lessons: ["m1.a"] } });
    expect(newlyEarned(defs, awarded, catalog)).toEqual([]); // never awarded twice
  });
  it("is pure: awarding does not mutate the input state", () => {
    const s = withLessons({ "m1.a": complete() });
    awardBadges(s, [{ id: "first", lessons: [] }], "t");
    expect(s.badges).toEqual({});
  });
  it("flags rules that reference content that does not exist", () => {
    expect(isAvailable({ type: "module-complete", module: "m1" }, catalog)).toBe(true);
    expect(isAvailable({ type: "module-complete", module: "core.generics" }, catalog)).toBe(false);
    expect(isAvailable({ type: "lesson-count", where: { module: "core.heroes" }, atLeast: 3 }, catalog)).toBe(false);
    expect(isAvailable({ type: "tier-complete", tier: "clean", atLeast: 1 }, catalog)).toBe(false);
    expect(isAvailable({ type: "all", rules: [{ type: "module-complete", module: "m1" }, { type: "module-complete", module: "zzz" }] }, catalog)).toBe(false);
  });
});

describe("checkRule", () => {
  it("accepts well-formed rules and reports malformed ones", () => {
    expect(checkRule({ type: "streak-days", atLeast: 3 })).toEqual([]);
    expect(checkRule({ type: "bogus" })[0]).toMatch(/unknown rule type/);
    expect(checkRule({ type: "module-complete" })[0]).toMatch(/needs module/);
    expect(checkRule({ type: "all", rules: [{ type: "tier-complete", tier: "works" }] })[0]).toMatch(/atLeast/);
  });
});

describe("track-complete (domain tracks)", () => {
  const domain: Catalog = {
    lessons: [
      { id: "d.a", module: "domain.games", type: "challenge", tiers: T(true, true, true, true) },
      { id: "d.b", module: "domain.games", type: "challenge", tiers: T(true, true, true, true) },
      { id: "e.a", module: "domain.finance", type: "challenge", tiers: T(true, true, true, true) },
    ],
  };
  const works = { works: true, robust: false, fast: false, clean: false };
  const rule: Rule = { type: "track-complete", module: "domain.games", tier: "works" };
  it("needs the named tier on EVERY challenge of the track, and nothing from other tracks", () => {
    const one = withLessons({ "d.a": complete({ bestTiers: works }) });
    expect(evaluate(rule, one, domain)).toMatchObject({ met: false, current: 1, target: 2 });
    const both = withLessons({ "d.a": complete({ bestTiers: works }), "d.b": complete({ bestTiers: works }) });
    const r = evaluate(rule, both, domain);
    expect(r.met).toBe(true);
    expect(r.lessons.sort()).toEqual(["d.a", "d.b"]);
    expect(evaluate({ type: "track-complete", module: "domain.finance", tier: "works" }, both, domain).met).toBe(false);
  });
  it("without a tier it needs every challenge complete", () => {
    const s = withLessons({ "d.a": complete(), "d.b": { ...emptyLesson(), status: "in_progress" } });
    expect(evaluate({ type: "track-complete", module: "domain.games" }, s, domain).met).toBe(false);
    expect(evaluate({ type: "track-complete", module: "domain.games" }, withLessons({ "d.a": complete(), "d.b": complete() }), domain).met).toBe(true);
  });
  it("is 'coming soon' for a track that has no challenges yet, and structurally checked", () => {
    expect(isAvailable({ type: "track-complete", module: "domain.embedded", tier: "works" }, domain)).toBe(false);
    expect(isAvailable(rule, domain)).toBe(true);
    expect(checkRule(rule)).toEqual([]);
    expect(checkRule({ type: "track-complete" })).not.toEqual([]);
    expect(evaluate({ type: "track-complete", module: "nope", tier: "works" }, emptyProgress(), domain).met).toBe(false);
  });
});
