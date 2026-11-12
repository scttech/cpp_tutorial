import { describe, expect, it } from "vitest";
import { emptyProgress, normalize } from "./model";
import { applyRun, countCodeLines } from "./recorder";
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

describe("applyRun: Off By One, Wall of Warnings, Refactorer", () => {
  const warn = (n: number) => Array.from({ length: n }, (_, i) => ({ severity: "warning" as const, file: "main.cpp", line: i + 1, column: 1, message: `w${i}` }));
  const solved = [pass("works"), pass("robust")];
  const build = (s: ReturnType<typeof emptyProgress>, n: number, mode: "run" | "test" = "run", compiled = true) =>
    applyRun(s, input({ mode, compiled, result: { diagnostics: warn(n) } as never }), NOON).state;
  it("Off By One: a failing test carrying the tag fires the event, a passing one or an untagged failure does not", () => {
    const tagged = { name: "index_of checks the last element too (off-by-one)", status: "fail" as const, tier: "works" as const, message: "x" };
    expect(applyRun(emptyProgress(), input({ tests: [tagged] }), NOON).state.events["off-by-one"]?.count).toBe(1);
    expect(applyRun(emptyProgress(), input({ tests: [{ ...tagged, status: "pass" as const }] }), NOON).state.events["off-by-one"]).toBeUndefined();
    expect(applyRun(emptyProgress(), input({ tests: [fail()] }), NOON).state.events["off-by-one"]).toBeUndefined();
  });
  it("Wall of Warnings: only a DROP between two successful builds of a lesson counts, by the size of the drop", () => {
    let s = build(emptyProgress(), 5);
    expect(s.events["warnings-fixed"]).toBeUndefined(); // the first build only sets the baseline
    s = build(s, 2);
    expect(s.events["warnings-fixed"]?.count).toBe(3);
    s = build(s, 4); // more warnings: nothing
    expect(s.events["warnings-fixed"]?.count).toBe(3);
    s = build(s, 0, "test");
    expect(s.events["warnings-fixed"]?.count).toBe(7);
    expect(s.lessons.l1?.lastWarnings).toBe(0);
  });
  it("Wall of Warnings: a failed build neither counts nor resets the baseline", () => {
    let s = build(emptyProgress(), 6);
    s = build(s, 0, "run", false);
    expect(s.lessons.l1?.lastWarnings).toBe(6);
    expect(s.events["warnings-fixed"]).toBeUndefined();
  });
  it("Refactorer: needs a passing run AFTER completion with fewer code lines than the best passing version", () => {
    let s = applyRun(emptyProgress(), input({ tests: solved, userLines: 30 }), NOON).state;
    expect(s.lessons.l1).toMatchObject({ status: "complete", bestLines: 30 });
    expect(s.events.refactor).toBeUndefined();
    s = applyRun(s, input({ tests: solved, userLines: 30 }), NOON).state; // the same size: nothing
    s = applyRun(s, input({ tests: solved, userLines: 35 }), NOON).state; // bigger: nothing
    expect(s.events.refactor).toBeUndefined();
    s = applyRun(s, input({ tests: [pass("works"), fail("robust")], userLines: 10 }), NOON).state; // shorter but failing: nothing
    expect(s.events.refactor).toBeUndefined();
    expect(s.lessons.l1?.bestLines).toBe(30);
    s = applyRun(s, input({ tests: solved, userLines: 22 }), NOON).state;
    expect(s.events.refactor?.count).toBe(1);
    expect(s.lessons.l1?.bestLines).toBe(22);
    s = applyRun(s, input({ tests: solved, userLines: 20 }), NOON).state;
    expect(s.events.refactor?.count).toBe(2);
  });
  it("Refactorer: lines before completion are not a refactor, and a lesson completed before the feature just gets a baseline", () => {
    let s = applyRun(emptyProgress(), input({ tests: [pass("works"), fail("robust")], userLines: 50 }), NOON).state;
    s = applyRun(s, input({ tests: solved, userLines: 20 }), NOON).state;
    expect(s.events.refactor).toBeUndefined();
    expect(s.lessons.l1?.bestLines).toBe(20);
    const old = normalize({ lessons: { l1: { status: "complete", bestTiers: { works: true, robust: true } } } });
    const after = applyRun(old, input({ tests: solved, userLines: 40 }), NOON).state;
    expect(after.events.refactor).toBeUndefined();
    expect(after.lessons.l1?.bestLines).toBe(40);
  });
  it("without userLines nothing about refactoring is tracked, and the new fields survive normalize", () => {
    expect(applyRun(emptyProgress(), input({ tests: solved }), NOON).state.lessons.l1?.bestLines).toBeUndefined();
    const n = normalize({ lessons: { l1: { bestLines: 12, lastWarnings: 3 } } }).lessons.l1;
    expect(n).toMatchObject({ bestLines: 12, lastWarnings: 3 });
    expect(normalize({ lessons: { l1: { bestLines: -4, lastWarnings: "x" } } }).lessons.l1?.lastWarnings).toBeUndefined();
  });
});

describe("countCodeLines", () => {
  it("counts code lines of the editable files only: blank lines and whole-line comments do not count", () => {
    const files = { "a.cpp": "int x;\n\n  // note\nint y; // trailing comment still counts\n", "b.hpp": "#pragma once\n", "main.cpp": "int main() {}\n" };
    expect(countCodeLines(files, ["a.cpp", "b.hpp"])).toBe(3);
    expect(countCodeLines(files, ["missing.cpp"])).toBe(0);
  });
});

describe("applyRun: Good Question", () => {
  const solved = [pass("works"), pass("robust")];
  const withLesson = (over: Record<string, unknown>) => normalize({ lessons: { l1: { ...over } } });
  it("fires when the learner asked for an explanation and then passed without revealing the solution", () => {
    const r = applyRun(withLesson({ explainUsed: true, aiHelpUsed: 1 }), input({ tests: solved }), NOON);
    expect(r.state.events["good-question"]?.count).toBe(1);
  });
  it("does not fire without an explain request, after a reveal, or on a later re-run", () => {
    expect(applyRun(withLesson({ aiHelpUsed: 2 }), input({ tests: solved }), NOON).state.events["good-question"]).toBeUndefined();
    expect(applyRun(withLesson({ explainUsed: true, solutionRevealed: true }), input({ tests: solved }), NOON).state.events["good-question"]).toBeUndefined();
    const once = applyRun(withLesson({ explainUsed: true }), input({ tests: solved }), NOON).state;
    expect(applyRun(once, input({ tests: solved }), NOON).state.events["good-question"]?.count).toBe(1);
  });
  it("explainUsed survives normalize and defaults to false", () => {
    expect(normalize({ lessons: { l1: { explainUsed: true } } }).lessons.l1?.explainUsed).toBe(true);
    expect(normalize({ lessons: { l1: { explainUsed: "yes" } } }).lessons.l1?.explainUsed).toBe(false);
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
