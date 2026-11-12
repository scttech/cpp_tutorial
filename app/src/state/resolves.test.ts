import { describe, expect, it } from "vitest";
import type { RunResult } from "../runner/types";
import { weakSpots } from "../warmup/weak";
import { emptyLesson, emptyProgress, normalize } from "./model";
import type { Progress } from "./model";
import { applyResolve, dueResolves, isClean, isResolveDue, passedAll, resolveFor } from "./resolves";

const day = (d: number) => new Date(2026, 4, d, 12); // May d, 2026, local noon
const done = (id: string, on: Date): Progress["lessons"] => ({ [id]: { ...emptyLesson(), status: "complete", firstCompleteAt: on.toISOString() } });
const progress = (): Progress => ({ ...emptyProgress(), lessons: { ...done("a", day(1)), ...done("b", day(2)), c: { ...emptyLesson(), status: "in_progress" } } });
const clean = { passed: true, hintsUsed: 0, solutionRevealed: false, ms: 90_000 };

describe("resolveFor", () => {
  it("only a completed lesson has a re-solve; one passed before this existed is due 3 days after it was completed", () => {
    const p = progress();
    expect(resolveFor(p, "c")).toBeNull();
    expect(resolveFor(p, "zzz")).toBeNull();
    expect(resolveFor(p, "a")).toMatchObject({ box: 1, due: "2026-05-04", count: 0 });
  });
  it("is due on the due date and after, not before", () => {
    const e = resolveFor(progress(), "a")!;
    expect(isResolveDue(e, "2026-05-03")).toBe(false);
    expect(isResolveDue(e, "2026-05-04")).toBe(true);
    expect(isResolveDue(e, "2026-06-01")).toBe(true);
  });
});

describe("dueResolves", () => {
  it("lists what is due, the most overdue first", () => {
    expect(dueResolves(progress(), "2026-05-05").map((d) => d.lessonId)).toEqual(["a", "b"]);
    expect(dueResolves(progress(), "2026-05-04").map((d) => d.lessonId)).toEqual(["a"]);
    expect(dueResolves(progress(), "2026-05-03")).toEqual([]);
  });
});

describe("applyResolve", () => {
  it("clean re-solves climb 3, 7, 14, 30 days and stay at 30", () => {
    let p = progress();
    const gaps: string[] = [];
    for (let i = 0; i < 5; i++) {
      p = applyResolve(p, "a", clean, day(10));
      gaps.push(p.resolves.a!.due);
    }
    expect(gaps).toEqual(["2026-05-17", "2026-05-24", "2026-06-09", "2026-06-09", "2026-06-09"]);
    expect(p.resolves.a!.count).toBe(5);
  });

  it("the first clean re-solve moves to the 7-day box", () => {
    const p = applyResolve(progress(), "a", clean, day(10));
    expect(p.resolves.a).toMatchObject({ box: 2, due: "2026-05-17", count: 1, misses: 0, bestMs: 90_000 });
  });

  it("a hint, the solution or giving up is a miss: back to 3 days, and the weak-spot tally records a wrong answer", () => {
    for (const o of [
      { passed: true, hintsUsed: 1, solutionRevealed: false, ms: 1 },
      { passed: true, hintsUsed: 0, solutionRevealed: true, ms: 1 },
      { passed: false, hintsUsed: 0, solutionRevealed: false, ms: 1 },
    ]) {
      expect(isClean(o)).toBe(false);
      let p = applyResolve(applyResolve(progress(), "a", clean, day(10)), "a", clean, day(17));
      expect(p.resolves.a!.box).toBe(3);
      p = applyResolve(p, "a", o, day(31));
      expect(p.resolves.a).toMatchObject({ box: 1, due: "2026-06-03", misses: 1, count: 3 });
      expect(p.weak["lesson:a"]).toEqual({ misses: 1, run: 0, last: "2026-05-31" });
    }
  });

  it("a clean re-solve is a right answer in the weak-spot tally", () => {
    const p = applyResolve(progress(), "a", clean, day(10));
    expect(p.weak["lesson:a"]).toEqual({ misses: 0, run: 1, last: "" });
  });

  it("keeps the best time, counts the day for the streak and the events", () => {
    let p = applyResolve(progress(), "a", { ...clean, ms: 80_000 }, day(10));
    p = applyResolve(p, "a", { ...clean, ms: 120_000 }, day(17));
    expect(p.resolves.a!.bestMs).toBe(80_000);
    p = applyResolve(p, "a", { passed: false, hintsUsed: 0, solutionRevealed: false, ms: 5 }, day(24));
    expect(p.resolves.a!.bestMs).toBe(80_000);
    expect(p.events["resolve-done"]!.count).toBe(3);
    expect(p.events["resolve-clean"]!.count).toBe(2);
    expect(p.streak.lastActiveDate).toBe("2026-05-24");
  });

  it("does nothing for a lesson that is not complete, and never touches the saved code or tiers", () => {
    const p = progress();
    expect(applyResolve(p, "c", clean, day(10))).toBe(p);
    p.lessons.a!.lastCode = { "main.cpp": "my solution" };
    const after = applyResolve(p, "a", clean, day(10));
    expect(after.lessons).toEqual(p.lessons);
  });
});

describe("a failed re-solve of a lesson with no warm-up cards", () => {
  it("shows as a weak spot to re-solve, and clears after N clean re-solves", () => {
    let p = applyResolve(progress(), "a", { passed: false, hintsUsed: 0, solutionRevealed: false, ms: 1 }, day(10));
    const input = { cards: [], weak: p.weak, friction: {}, completed: new Set(["a", "b"]), titleOf: (id: string) => `Title ${id}`, clearAfter: 2, today: "2026-05-10" };
    const spots = weakSpots(input);
    expect(spots.map((s) => [s.unit, s.cards, s.title])).toEqual([["lesson:a", 0, "Title a"]]);
    p = applyResolve(p, "a", clean, day(13));
    expect(weakSpots({ ...input, weak: p.weak })).toHaveLength(1);
    p = applyResolve(p, "a", clean, day(20));
    expect(weakSpots({ ...input, weak: p.weak })).toEqual([]);
  });
});

describe("passedAll", () => {
  const run = (over: Partial<RunResult>): RunResult => ({ compiled: true, timedOut: false, tests: [{ name: "t", status: "pass" }], ...over }) as RunResult;
  it("needs a build, no timeout, at least one test and every test passing", () => {
    expect(passedAll(run({}))).toBe(true);
    expect(passedAll(run({ compiled: false }))).toBe(false);
    expect(passedAll(run({ timedOut: true }))).toBe(false);
    expect(passedAll(run({ tests: [] }))).toBe(false);
    expect(passedAll(run({ tests: [{ name: "a", status: "pass" }, { name: "b", status: "fail" }] as RunResult["tests"] }))).toBe(false);
  });
});

describe("normalize", () => {
  it("keeps good re-solve entries, clamps the box and drops bad ones", () => {
    const n = normalize({ resolves: { a: { box: 9, due: "2026-05-04", count: 2, misses: 1, lastAt: "x", bestMs: 5 }, b: { due: "soon" }, c: "junk" } });
    expect(n.resolves.a).toEqual({ box: 4, due: "2026-05-04", count: 2, misses: 1, lastAt: "x", bestMs: 5 });
    expect(n.resolves.b).toBeUndefined();
    expect(n.resolves.c).toBeUndefined();
    expect(normalize({}).resolves).toEqual({});
  });
});
