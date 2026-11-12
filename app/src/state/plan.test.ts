import { describe, expect, it } from "vitest";
import { addDays } from "../warmup/schedule";
import { activityOn, bumpActivity, KEEP_DAYS } from "./activity";
import { emptyLesson, emptyProgress, normalize } from "./model";
import type { Progress } from "./model";
import { makePlan, planComplete, planItems, PLAN_WARMUPS } from "./plan";
import { applyResolve } from "./resolves";
import { applyReview } from "./reviews";

const today = "2026-05-10";
const noon = new Date(2026, 4, 10, 12);
const lesson = (status: "available" | "in_progress" | "complete", firstCompleteAt?: string) => ({ ...emptyLesson(), status, ...(firstCompleteAt ? { firstCompleteAt } : {}) });
/** A learner with one lesson passed long ago (so a re-solve is due), one in progress, and the plan made for today. */
const base = (): Progress => ({ ...emptyProgress(), lessons: { old: lesson("complete", new Date(2026, 3, 1, 12).toISOString()), cur: lesson("in_progress") } });

describe("activity", () => {
  it("counts per day and per kind", () => {
    let p = bumpActivity(emptyProgress(), today, "warmups");
    p = bumpActivity(p, today, "warmups");
    p = bumpActivity(p, today, "runs", 3);
    p = bumpActivity(p, "2026-05-09", "lessons");
    expect(activityOn(p, today)).toEqual({ warmups: 2, resolves: 0, lessons: 0, runs: 3, ms: 0 });
    expect(activityOn(p, "2026-05-09").lessons).toBe(1);
    expect(activityOn(p, "2020-01-01")).toEqual({ warmups: 0, resolves: 0, lessons: 0, runs: 0, ms: 0 });
  });
  it("forgets days older than the window", () => {
    let p = bumpActivity(emptyProgress(), "2026-01-01", "runs");
    p = bumpActivity(p, addDays("2026-01-01", KEEP_DAYS), "runs");
    expect(Object.keys(p.activity)).toContain("2026-01-01");
    p = bumpActivity(p, addDays("2026-01-01", KEEP_DAYS + 1), "runs");
    expect(Object.keys(p.activity)).not.toContain("2026-01-01");
  });
  it("answering a warm-up card and finishing a re-solve are counted for the day", () => {
    let p = applyReview(base(), "card", true, noon);
    expect(activityOn(p, today).warmups).toBe(1);
    p = applyResolve(p, "old", { passed: true, hintsUsed: 0, solutionRevealed: false, ms: 5 }, noon);
    expect(activityOn(p, today).resolves).toBe(1);
  });
  it("normalize keeps good days and drops bad ones", () => {
    const n = normalize({ activity: { "2026-05-10": { warmups: 2, runs: -1, lessons: "x" }, tomorrow: { warmups: 1 }, "2026-05-09": 5 } });
    expect(n.activity).toEqual({ "2026-05-10": { warmups: 2, resolves: 0, lessons: 0, runs: 0, ms: 0 } });
  });
});

describe("makePlan", () => {
  it("takes the next lesson and the most overdue re-solve", () => {
    expect(makePlan(base(), today, "next")).toEqual({ date: today, lessonId: "next", resolveId: "old" });
  });
  it("leaves out what does not exist", () => {
    expect(makePlan(emptyProgress(), today, null)).toEqual({ date: today });
  });
});

describe("planItems", () => {
  const planned = (over: Partial<Progress> = {}): Progress => ({ ...base(), plan: { date: today, lessonId: "cur", resolveId: "old" }, ...over });

  it("has no steps before a plan exists for today", () => {
    expect(planItems(base(), today, true)).toEqual([]);
    expect(planItems(planned({ plan: { date: "2026-05-09", lessonId: "cur" } }), today, true)).toEqual([]);
  });

  it("warm-up, re-solve, then the new lesson, all still to do", () => {
    const items = planItems(planned(), today, true);
    expect(items.map((i) => [i.id, i.done])).toEqual([["warmup", false], ["resolve", false], ["lesson", false]]);
    expect(items[0]!.detail).toBe(`0 of ${PLAN_WARMUPS} cards`);
    expect(items[2]!.detail).toBe("in progress");
    expect(planComplete(items)).toBe(false);
  });

  it("the warm-up step is done after a set of cards", () => {
    let p = planned();
    for (let i = 0; i < PLAN_WARMUPS - 1; i++) p = applyReview(p, `c${i}`, true, noon);
    expect(planItems(p, today, true)[0]).toMatchObject({ done: false, detail: `${PLAN_WARMUPS - 1} of ${PLAN_WARMUPS} cards` });
    p = applyReview(p, "last", false, noon);
    expect(planItems(p, today, true)[0]!.done).toBe(true);
  });

  it("the warm-up step is left out when there is nothing to ask, but kept once cards were answered", () => {
    expect(planItems(planned(), today, false).map((i) => i.id)).toEqual(["resolve", "lesson"]);
    expect(planItems(applyReview(planned(), "c", true, noon), today, false).map((i) => i.id)).toContain("warmup");
  });

  it("the re-solve step is done once the lesson was re-solved today, even if it was a miss", () => {
    const p = applyResolve(planned(), "old", { passed: false, hintsUsed: 0, solutionRevealed: false, ms: 1 }, noon);
    expect(planItems(p, today, true).find((i) => i.id === "resolve")).toMatchObject({ done: true, detail: "re-solved today" });
  });

  it("the new lesson stays in the plan, as done, after it is completed", () => {
    const p = planned({ lessons: { ...planned().lessons, cur: lesson("complete", noon.toISOString()) } });
    expect(planItems(p, today, true).find((i) => i.id === "lesson")).toMatchObject({ id: "lesson", lessonId: "cur", done: true, detail: "completed" });
  });

  it("the plan is complete when every step is done", () => {
    let p = planned({ lessons: { ...planned().lessons, cur: lesson("complete", noon.toISOString()) } });
    p = applyResolve(p, "old", { passed: true, hintsUsed: 0, solutionRevealed: false, ms: 1 }, noon);
    for (let i = 0; i < PLAN_WARMUPS; i++) p = applyReview(p, `c${i}`, true, noon);
    expect(planComplete(planItems(p, today, true))).toBe(true);
  });
});

describe("normalize the plan", () => {
  it("keeps a valid plan and drops a bad one", () => {
    expect(normalize({ plan: { date: "2026-05-10", lessonId: "a", resolveId: "b", junk: 1 } }).plan).toEqual({ date: "2026-05-10", lessonId: "a", resolveId: "b" });
    expect(normalize({ plan: { date: "today" } }).plan).toBeNull();
    expect(normalize({}).plan).toBeNull();
  });
});
