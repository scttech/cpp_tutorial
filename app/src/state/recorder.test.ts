import { describe, expect, it } from "vitest";
import { emptyProgress, normalize } from "./model";
import { applyRun } from "./recorder";
import type { RunInput } from "./recorder";

const NOON = new Date(2026, 4, 5, 12, 0, 0);
const pass = (tier: "works" | "robust" = "works") => ({ name: `t-${tier}`, status: "pass" as const, tier });
const fail = (tier: "works" | "robust" = "works") => ({ name: `t-${tier}`, status: "fail" as const, tier, message: "nope" });
const input = (over: Partial<RunInput> & { tests?: RunInput["result"]["tests"]; compiled?: boolean } = {}): RunInput => ({
  lessonId: "l1",
  mode: "test",
  declaredTiers: ["works", "robust"],
  sessionCompileErrors: 0,
  ...over,
  result: {
    compiled: over.compiled ?? true,
    tests: over.tests ?? [],
    signal: null,
    timedOut: false,
    diagnostics: [],
    ...(over.result ?? {}),
  },
});

describe("applyRun: tiers and completion", () => {
  it("earns a tier only when ALL tests of that tier pass; completes when all declared tiers are earned", () => {
    let s = emptyProgress();
    s = applyRun(s, input({ tests: [pass("works"), fail("robust")] }), NOON).state;
    expect(s.lessons.l1).toMatchObject({ status: "in_progress", bestTiers: { works: true, robust: false }, testRuns: 1, failedRuns: 1 });
    const done = applyRun(s, input({ tests: [pass("works"), pass("robust")] }), NOON);
    expect(done.becameComplete).toBe(true);
    expect(done.state.lessons.l1).toMatchObject({ status: "complete", runsToFirstPass: 2, firstCompleteAt: NOON.toISOString() });
  });
  it("a first-run pass records runsToFirstPass = 1", () => {
    const r = applyRun(emptyProgress(), input({ tests: [pass("works"), pass("robust")] }), NOON);
    expect(r.state.lessons.l1?.runsToFirstPass).toBe(1);
    expect(r.state.lessons.l1?.failedRuns).toBe(0);
  });
  it("never downgrades a completed lesson, and stops counting failures after completion", () => {
    const done = applyRun(emptyProgress(), input({ tests: [pass("works"), pass("robust")] }), NOON).state;
    const again = applyRun(done, input({ tests: [fail("works"), fail("robust")] }), NOON);
    expect(again.becameComplete).toBe(false);
    expect(again.state.lessons.l1).toMatchObject({ status: "complete", failedRuns: 0, bestTiers: { works: true, robust: true } });
  });
  it("a plain Run counts as an attempt but never earns tiers or counts as a failed test run", () => {
    const r = applyRun(emptyProgress(), input({ mode: "run", tests: [] }), NOON).state.lessons.l1!;
    expect(r).toMatchObject({ attempts: 1, testRuns: 0, failedRuns: 0, status: "in_progress" });
    expect(r.bestTiers.works).toBe(false);
  });
  it("a run that the time limit killed earns nothing: only the tests that finished are reported, and all of them may have passed", () => {
    const r = applyRun(emptyProgress(), input({ tests: [pass("works"), pass("robust")], result: { timedOut: true } as never }), NOON);
    expect(r.becameComplete).toBe(false);
    expect(r.state.lessons.l1).toMatchObject({ status: "in_progress", bestTiers: { works: false, robust: false }, failedRuns: 1 });
  });
  it("a compile failure on a test run is a failed run", () => {
    const r = applyRun(emptyProgress(), input({ compiled: false }), NOON).state.lessons.l1!;
    expect(r.failedRuns).toBe(1);
  });
  it("is pure: the input state is not mutated", () => {
    const s = emptyProgress();
    applyRun(s, input({ tests: [pass()] }), NOON);
    expect(s.lessons).toEqual({});
  });
});

describe("applyRun: streak and events", () => {
  it("starts the streak", () => {
    const s = applyRun(emptyProgress(), input({ mode: "run" }), NOON).state;
    expect(s.streak).toMatchObject({ current: 1, lastActiveDate: "2026-05-05" });
  });
  it("records segfault and timeout events", () => {
    let s = applyRun(emptyProgress(), input({ mode: "run", result: { signal: "SIGSEGV" } as never }), NOON).state;
    expect(s.events.segfault?.count).toBe(1);
    s = applyRun(s, input({ mode: "run", result: { timedOut: true } as never }), NOON).state;
    expect(s.events.timeout?.count).toBe(1);
    s = applyRun(s, input({ tests: [{ name: "x", status: "error", message: "crashed: Segmentation fault" }] }), NOON).state;
    expect(s.events.segfault?.count).toBe(2);
    s = applyRun(s, input({ tests: [{ name: "x", status: "timeout" }] }), NOON).state;
    expect(s.events.timeout?.count).toBe(2);
  });
  it("It Compiles!: a clean compile after 10+ errors in the session fires the event once", () => {
    const err = Array.from({ length: 6 }, () => ({ severity: "error" as const, file: "a", line: 1, column: 1, message: "m" }));
    let r = applyRun(emptyProgress(), input({ compiled: false, result: { diagnostics: err } as never }), NOON);
    expect(r.sessionCompileErrors).toBe(6);
    r = applyRun(r.state, input({ compiled: false, sessionCompileErrors: r.sessionCompileErrors, result: { diagnostics: err } as never }), NOON);
    expect(r.sessionCompileErrors).toBe(12);
    r = applyRun(r.state, input({ compiled: true, sessionCompileErrors: r.sessionCompileErrors, tests: [pass()] }), NOON);
    expect(r.state.events["compiled-after-many-errors"]?.count).toBe(1);
    expect(r.sessionCompileErrors).toBe(0);
  });
  it("does not fire It Compiles! for only a few errors", () => {
    const r = applyRun(emptyProgress(), input({ compiled: true, sessionCompileErrors: 9, tests: [pass()] }), NOON);
    expect(r.state.events["compiled-after-many-errors"]).toBeUndefined();
  });
  it("Night Owl / Early Bird fire only when a lesson is first completed in that window", () => {
    const all = [pass("works"), pass("robust")];
    const night = applyRun(emptyProgress(), input({ tests: all }), new Date(2026, 4, 5, 2, 30)).state;
    expect(night.events["completed-at-night"]?.count).toBe(1);
    const early = applyRun(emptyProgress(), input({ tests: all }), new Date(2026, 4, 5, 5, 0)).state;
    expect(early.events["completed-early-morning"]?.count).toBe(1);
    const day = applyRun(emptyProgress(), input({ tests: all }), NOON).state;
    expect(day.events["completed-at-night"]).toBeUndefined();
    // a later re-run at night does not count again
    const rerun = applyRun(day, input({ tests: all }), new Date(2026, 4, 6, 2, 0)).state;
    expect(rerun.events["completed-at-night"]).toBeUndefined();
  });
});

describe("normalize (untrusted input)", () => {
  it("returns defaults for garbage and never throws", () => {
    for (const bad of [null, undefined, 5, "x", [], { lessons: 7 }, { lessons: { a: 3 } }, { streak: "no" }]) {
      expect(() => normalize(bad)).not.toThrow();
    }
    expect(normalize(null)).toEqual(emptyProgress());
  });
  it("migrates the first-release shape (no streak/badges/events/failedRuns) and keeps the data", () => {
    const old = { version: 1, lessons: { l1: { status: "complete", bestTiers: { works: true }, attempts: 4, testRuns: 3, hintsUsed: 1, solutionRevealed: false, lastCode: { "main.cpp": "x" }, firstCompleteAt: "2026-01-01T00:00:00Z" } } };
    const n = normalize(old);
    expect(n.lessons.l1).toMatchObject({ status: "complete", attempts: 4, hintsUsed: 1, failedRuns: 0, lastCode: { "main.cpp": "x" } });
    expect(n.lessons.l1?.bestTiers).toEqual({ works: true, robust: false, fast: false, clean: false });
    expect(n.streak.current).toBe(0);
    expect(n.badges).toEqual({});
  });
  it("drops unknown fields and wrong types, and rejects negative or non-finite numbers", () => {
    const n = normalize({ lessons: { l1: { attempts: -3, testRuns: Infinity, hintsUsed: "9", status: "weird", evil: { x: 1 } } }, extra: true });
    expect(n.lessons.l1).toMatchObject({ attempts: 0, testRuns: 0, hintsUsed: 0, status: "available" });
    expect(n.lessons.l1).not.toHaveProperty("evil");
    expect(n).not.toHaveProperty("extra");
  });
  it("round-trips a fully populated state through JSON", () => {
    const filled = applyRun(emptyProgress(), input({ tests: [pass("works"), pass("robust")] }), NOON).state;
    expect(normalize(JSON.parse(JSON.stringify(filled)))).toEqual(filled);
  });
});
