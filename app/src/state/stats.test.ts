import { describe, expect, it } from "vitest";
import { bumpActivity } from "./activity";
import { emptyLesson, emptyProgress, normalize } from "./model";
import type { LessonProgress, Progress } from "./model";
import { applyRun, tallyErrors } from "./recorder";
import { accuracyByTopic, dayPoints, effortRanking, formatDuration, heatmap, level, summarize, topErrors, weekly } from "./stats";
import { applyTime, MAX_SLICE_MS } from "./time";

const done = (over: Partial<LessonProgress>): LessonProgress => ({ ...emptyLesson(), status: "complete", ...over });
const day = (d: number) => new Date(2026, 4, d, 12);

describe("summarize", () => {
  it("is empty for a new learner", () => {
    expect(summarize(emptyProgress())).toMatchObject({ completed: 0, measured: 0, firstTryRate: null, medianTries: null, totalMs: 0 });
  });
  it("first-try rate, median tries and total time", () => {
    const p: Progress = {
      ...emptyProgress(),
      lessons: {
        a: done({ runsToFirstPass: 1, timeMs: 60_000 }),
        b: done({ runsToFirstPass: 1, timeMs: 30_000 }),
        c: done({ runsToFirstPass: 5, timeMs: 10_000 }),
        d: done({}), // passed before the numbers were recorded: counted as completed, not measured
        e: { ...emptyLesson(), status: "in_progress", timeMs: 5_000 },
      },
    };
    const s = summarize(p);
    expect(s).toMatchObject({ completed: 4, measured: 3, medianTries: 1, totalMs: 105_000 });
    expect(s.firstTryRate).toBeCloseTo(2 / 3);
    expect(summarize({ ...p, lessons: { x: done({ runsToFirstPass: 2 }), y: done({ runsToFirstPass: 4 }) } }).medianTries).toBe(3);
  });
});

describe("effortRanking", () => {
  it("ranks by tries, time, hints and the solution; leaves out easy and unfinished lessons", () => {
    const p: Progress = {
      ...emptyProgress(),
      lessons: {
        easy: done({ runsToFirstPass: 1, msToFirstPass: 20_000 }),
        retries: done({ runsToFirstPass: 6, msToFirstPass: 5 * 60_000 }),
        peeked: done({ runsToFirstPass: 2, solutionRevealed: true, hintsUsed: 2 }),
        slow: done({ runsToFirstPass: 1, msToFirstPass: 40 * 60_000 }),
        open: { ...emptyLesson(), status: "in_progress", hintsUsed: 9 },
      },
    };
    expect(effortRanking(p).map((e) => e.lessonId)).toEqual(["slow", "peeked", "retries"]);
    expect(effortRanking(p, 1)).toHaveLength(1);
  });
});

describe("topErrors", () => {
  it("most frequent first, with kind and id", () => {
    let p = tallyErrors(emptyProgress(), ["compile:missing-semicolon", "runtime:AddressSanitizer: heap-use-after-free"], day(1).toISOString());
    p = tallyErrors(p, ["compile:missing-semicolon"], day(2).toISOString());
    p = tallyErrors(p, ["compile:other"], day(3).toISOString());
    const top = topErrors(p);
    expect(top.map((e) => [e.kind, e.id, e.count])).toEqual([["compile", "missing-semicolon", 2], ["compile", "other", 1], ["runtime", "AddressSanitizer: heap-use-after-free", 1]]);
  });
  it("remembers only the most common kinds", () => {
    let p = emptyProgress();
    p = tallyErrors(p, ["compile:common"], day(1).toISOString());
    p = tallyErrors(p, ["compile:common"], day(1).toISOString());
    for (let i = 0; i < 150; i++) p = tallyErrors(p, [`compile:rare-${i}`], day(2).toISOString());
    expect(Object.keys(p.errors)).toHaveLength(100);
    expect(p.errors["compile:common"]!.count).toBe(2);
  });
});

describe("accuracyByTopic", () => {
  it("adds up cards by topic, weakest first, skipping cards that no longer exist", () => {
    const p: Progress = { ...emptyProgress(), reviews: { a: { box: 1, due: "2026-05-10", seen: 4, correct: 1 }, b: { box: 1, due: "2026-05-10", seen: 2, correct: 2 }, c: { box: 1, due: "2026-05-10", seen: 2, correct: 1 }, gone: { box: 1, due: "2026-05-10", seen: 9, correct: 0 } } };
    const t = { a: "Complexity", b: "Complexity", c: "Gotchas" } as Record<string, string>;
    expect(accuracyByTopic(p, (id) => t[id])).toEqual([
      { topic: "Complexity", seen: 6, correct: 3, rate: 0.5 }, // same rate: the one with more answers first
      { topic: "Gotchas", seen: 2, correct: 1, rate: 0.5 },
    ]);
  });
});

describe("weekly", () => {
  it("this week is the last 7 days including today, last week the 7 before", () => {
    let p = bumpActivity(emptyProgress(), "2026-05-10", "runs", 4);
    p = bumpActivity(p, "2026-05-04", "runs", 1); // 6 days back: this week
    p = bumpActivity(p, "2026-05-03", "runs", 10); // 7 days back: last week
    p = bumpActivity(p, "2026-04-27", "warmups", 5); // 13 days back: last week
    p = bumpActivity(p, "2026-04-26", "runs", 99); // 14 days back: neither
    const w = weekly(p, "2026-05-10");
    expect(w.thisWeek.runs).toBe(5);
    expect(w.lastWeek).toMatchObject({ runs: 10, warmups: 5 });
  });
});

describe("heatmap", () => {
  it("is 12 columns of 7 days, Sunday on top, ending with the week of today (a Sunday here)", () => {
    const cols = heatmap(emptyProgress(), "2026-05-10", 0); // 2026-05-10 is a Sunday
    expect(cols).toHaveLength(12);
    expect(cols.every((c) => c.length === 7)).toBe(true);
    expect(cols[11]![0]!.date).toBe("2026-05-10");
    expect(cols[0]![0]!.date).toBe("2026-02-22");
    expect(cols[11]!.slice(1).every((d) => d.future)).toBe(true);
  });
  it("a Wednesday: the days after it are future and the week starts on its Sunday", () => {
    const cols = heatmap(emptyProgress(), "2026-05-13", 3);
    expect(cols[11]![0]!.date).toBe("2026-05-10");
    expect(cols[11]!.map((d) => d.future)).toEqual([false, false, false, false, true, true, true]);
  });
  it("describes a day in words and grades it", () => {
    let p = bumpActivity(emptyProgress(), "2026-05-10", "runs", 3);
    p = bumpActivity(p, "2026-05-10", "warmups", 5);
    p = bumpActivity(p, "2026-05-10", "lessons");
    const d = heatmap(p, "2026-05-10", 0)[11]![0]!;
    expect(d.text).toBe("3 runs, 5 cards, 1 lesson completed");
    expect(d.points).toBe(13);
    expect(d.level).toBe(2);
  });
  it("levels", () => {
    expect([0, 1, 4, 5, 14, 15, 29, 30, 99].map(level)).toEqual([0, 1, 1, 2, 2, 3, 3, 4, 4]);
    expect(dayPoints({ warmups: 1, resolves: 1, lessons: 1, runs: 1, ms: 5 })).toBe(9);
  });
});

describe("formatDuration", () => {
  it("reads naturally", () => {
    expect([30_000, 60_000, 12 * 60_000, 65 * 60_000, 3 * 3_600_000].map(formatDuration)).toEqual(["30 s", "1 min", "12 min", "1 h 05 min", "3 h 00 min"]);
  });
});

describe("recording time", () => {
  it("adds to the lesson and the day, capped per report, and creates the lesson entry", () => {
    let p = applyTime(emptyProgress(), "a", 90_000, day(10));
    expect(p.lessons.a!.timeMs).toBe(90_000);
    expect(p.activity["2026-05-10"]!.ms).toBe(90_000);
    p = applyTime(p, "a", 10 * 60_000, day(10));
    expect(p.lessons.a!.timeMs).toBe(90_000 + MAX_SLICE_MS);
    expect(applyTime(p, "a", 0, day(10))).toBe(p);
    expect(applyTime(p, "a", -5, day(10))).toBe(p);
  });
  it("the time at the first pass is remembered, and only the first", () => {
    const pass = { compiled: true, timedOut: false, tests: [{ name: "t", status: "pass" as const, tier: "works" as const }], signal: null, diagnostics: [] };
    const input = { lessonId: "a", mode: "test" as const, declaredTiers: ["works" as const], result: pass, sessionCompileErrors: 0 };
    let p = applyTime(emptyProgress(), "a", 120_000, day(10));
    p = applyRun(p, input, day(10)).state;
    expect(p.lessons.a).toMatchObject({ status: "complete", msToFirstPass: 120_000, runsToFirstPass: 1 });
    p = applyTime(p, "a", 60_000, day(11));
    p = applyRun(p, input, day(11)).state;
    expect(p.lessons.a!.msToFirstPass).toBe(120_000);
  });
});

describe("recording what goes wrong", () => {
  const input = (result: object) => ({ lessonId: "a", mode: "test" as const, declaredTiers: ["works" as const], sessionCompileErrors: 0, result: { compiled: false, timedOut: false, tests: [], signal: null, diagnostics: [], ...result } as never });
  const err = (ruleId?: string) => ({ severity: "error" as const, file: "main.cpp", line: 1, column: 1, message: "x", ...(ruleId ? { ruleId } : {}) });

  it("counts each kind of compile error once per run, and unexplained ones as 'other'", () => {
    const r = applyRun(emptyProgress(), input({ diagnostics: [err("missing-semicolon"), err("missing-semicolon"), err("missing-semicolon"), err()] }), day(10));
    expect(Object.fromEntries(Object.entries(r.state.errors).map(([k, v]) => [k, v.count]))).toEqual({ "compile:missing-semicolon": 1, "compile:other": 1 });
  });
  it("warnings and notes are not errors, and a clean build records nothing", () => {
    const warn = { ...err("w"), severity: "warning" as const };
    expect(applyRun(emptyProgress(), input({ diagnostics: [warn] }), day(10)).state.errors).toEqual({});
    expect(applyRun(emptyProgress(), input({ compiled: true, diagnostics: [err("x")] }), day(10)).state.errors).toEqual({});
  });
  it("records a sanitizer finding", () => {
    const r = applyRun(emptyProgress(), input({ compiled: true, report: { summary: "AddressSanitizer: heap-use-after-free" } }), day(10));
    expect(Object.keys(r.state.errors)).toEqual(["runtime:AddressSanitizer: heap-use-after-free"]);
  });
});

describe("normalize the new fields", () => {
  it("keeps good values and drops bad ones", () => {
    const n = normalize({ lessons: { a: { status: "complete", timeMs: 5000, msToFirstPass: 1000 }, b: { timeMs: -3 } }, errors: { "compile:x": { count: 2, lastAt: "t" }, junk: { count: 1 }, "compile:y": "no" } });
    expect(n.lessons.a).toMatchObject({ timeMs: 5000, msToFirstPass: 1000 });
    expect(n.lessons.b!.timeMs).toBe(0);
    expect(n.errors).toEqual({ "compile:x": { count: 2, lastAt: "t" } });
    expect(normalize({}).errors).toEqual({});
  });
});
