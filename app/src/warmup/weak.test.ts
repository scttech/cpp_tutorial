import { describe, expect, it } from "vitest";
import { emptyProgress, normalize } from "../state/model";
import { applyReview } from "../state/reviews";
import type { Card } from "./cards";
import { CLEAR_RANGE, DEFAULT_CLEAR, frictionOf, pickPractice, recordAnswer, unitOf, weakSpots } from "./weak";
import type { WeakEntry, WeakInput } from "./weak";

const demo = (lesson: string, gotcha: string): Card => ({ id: `${lesson}/${gotcha}/predict`, source: "demo", topic: "Gotchas", prompt: "p", choices: ["a", "b"], answer: 0, explain: "e", lessonId: lesson, gotchaId: gotcha });
const cs = (id: string, topic: string): Card => ({ id, source: "cs", topic, prompt: "p", choices: ["a", "b"], answer: 0, explain: "e" });

const cards = [demo("l1", "g1"), demo("l1", "g2"), demo("l2", "g1"), cs("c1", "Boolean logic"), cs("c2", "Boolean logic")];
const today = "2026-05-10";
const base = (over: Partial<WeakInput> = {}): WeakInput => ({ cards, weak: {}, friction: {}, completed: new Set(["l1", "l2"]), titleOf: (id) => `Title ${id}`, clearAfter: 5, today, ...over });
const quiet = { hintsUsed: 0, solutionRevealed: false, failedRuns: 0, aiHelpUsed: 0 };

describe("recordAnswer", () => {
  it("a miss counts and resets the run; right answers build the run", () => {
    let w: Record<string, WeakEntry> = {};
    w = recordAnswer(w, "lesson:l1", false, today);
    w = recordAnswer(w, "lesson:l1", true, today);
    w = recordAnswer(w, "lesson:l1", true, today);
    expect(w["lesson:l1"]).toEqual({ misses: 1, run: 2, last: today });
    w = recordAnswer(w, "lesson:l1", false, "2026-05-11");
    expect(w["lesson:l1"]).toEqual({ misses: 2, run: 0, last: "2026-05-11" });
  });
  it("units: a lesson for gotcha cards, a topic for bank cards", () => {
    expect(unitOf(demo("l1", "g1"))).toBe("lesson:l1");
    expect(unitOf(cs("c1", "Boolean logic"))).toBe("topic:Boolean logic");
  });
});

describe("weakSpots", () => {
  it("nothing is weak before anything is missed", () => expect(weakSpots(base())).toEqual([]));

  it("a miss makes a unit weak, and it clears after N right answers in a row", () => {
    let w = recordAnswer({}, "lesson:l1", false, today);
    expect(weakSpots(base({ weak: w })).map((s) => s.unit)).toEqual(["lesson:l1"]);
    for (let i = 0; i < 4; i++) w = recordAnswer(w, "lesson:l1", true, today);
    const spot = weakSpots(base({ weak: w }))[0]!;
    expect(spot.run).toBe(4);
    expect(spot.need).toBe(5);
    w = recordAnswer(w, "lesson:l1", true, today);
    expect(weakSpots(base({ weak: w }))).toEqual([]);
  });

  it("a new miss after clearing brings it back", () => {
    let w: Record<string, WeakEntry> = recordAnswer({}, "lesson:l1", false, today);
    for (let i = 0; i < 5; i++) w = recordAnswer(w, "lesson:l1", true, today);
    w = recordAnswer(w, "lesson:l1", false, "2026-05-12");
    expect(weakSpots(base({ weak: w, today: "2026-05-12" })).length).toBe(1);
  });

  it("the clearing count is the setting: 1 clears at once, a higher one takes longer", () => {
    const w = recordAnswer(recordAnswer({}, "topic:Boolean logic", false, today), "topic:Boolean logic", true, today);
    expect(weakSpots(base({ weak: w, clearAfter: 1 }))).toEqual([]);
    expect(weakSpots(base({ weak: w, clearAfter: 2 })).length).toBe(1);
  });

  it("lesson friction alone makes a completed lesson weak; a little friction does not", () => {
    const hard = { hintsUsed: 3, solutionRevealed: true, failedRuns: 9, aiHelpUsed: 0 };
    const s = weakSpots(base({ friction: { l1: hard, l2: { ...quiet, hintsUsed: 2 } } }));
    expect(s.map((x) => x.unit)).toEqual(["lesson:l1"]);
    expect(s[0]!.reasons.join(" ")).toMatch(/solution/);
  });

  it("friction only counts for a lesson with cards, and a lesson that is not complete has none", () => {
    expect(weakSpots(base({ completed: new Set(["l2"]), weak: recordAnswer({}, "lesson:l1", false, today) }))).toEqual([]);
  });

  it("ranks by score: more misses and recent ones first", () => {
    let w: Record<string, WeakEntry> = {};
    w = recordAnswer(w, "lesson:l1", false, "2026-04-01");
    w = recordAnswer(w, "topic:Boolean logic", false, today);
    w = recordAnswer(w, "topic:Boolean logic", false, today);
    expect(weakSpots(base({ weak: w })).map((s) => s.unit)).toEqual(["topic:Boolean logic", "lesson:l1"]);
  });

  it("names a lesson unit by its lesson title and a topic by its topic", () => {
    const w = recordAnswer(recordAnswer({}, "lesson:l1", false, today), "topic:Boolean logic", false, today);
    const t = Object.fromEntries(weakSpots(base({ weak: w })).map((s) => [s.unit, s.title]));
    expect(t).toEqual({ "lesson:l1": "Title l1", "topic:Boolean logic": "Boolean logic" });
  });

  it("frictionOf scores what happened", () => {
    expect(frictionOf(quiet).points).toBe(0);
    expect(frictionOf({ ...quiet, solutionRevealed: true }).points).toBe(2);
  });
});

describe("pickPractice", () => {
  const input = { cards, reviews: {}, completed: new Set(["l1", "l2"]), units: new Set(["lesson:l1"]), today };
  it("only draws from the chosen units, one card per gotcha", () => {
    const set = pickPractice(input);
    expect(set.map((c) => c.id).sort()).toEqual(["l1/g1/predict", "l1/g2/predict"]);
  });
  it("cards missed before come first, even when they are not due", () => {
    const reviews = { "l1/g2/predict": { box: 1, due: "2026-06-30", seen: 2, correct: 0 }, "l1/g1/predict": { box: 4, due: "2026-06-30", seen: 3, correct: 3 } };
    expect(pickPractice({ ...input, reviews })[0]!.id).toBe("l1/g2/predict");
  });
  it("is deterministic and respects the size", () => {
    const all = { ...input, units: new Set(["lesson:l1", "lesson:l2", "topic:Boolean logic"]) };
    expect(pickPractice(all)).toEqual(pickPractice(all));
    expect(pickPractice({ ...all, size: 2 }).length).toBe(2);
  });
});

describe("progress integration", () => {
  it("applyReview keeps the tally when given a unit, and not otherwise", () => {
    const now = new Date(2026, 4, 10, 12);
    const a = applyReview(emptyProgress(), "x", false, now, "lesson:l1");
    expect(a.weak["lesson:l1"]).toEqual({ misses: 1, run: 0, last: "2026-05-10" });
    expect(applyReview(emptyProgress(), "x", false, now).weak).toEqual({});
  });
  it("normalize defaults and clamps the setting and drops bad tallies", () => {
    expect(normalize({}).settings.weakClear).toBe(DEFAULT_CLEAR);
    expect(normalize({ settings: { weakClear: 99 } }).settings.weakClear).toBe(CLEAR_RANGE.max);
    expect(normalize({ settings: { weakClear: 0 } }).settings.weakClear).toBe(CLEAR_RANGE.min);
    const n = normalize({ weak: { a: { misses: 2, run: 1, last: "2026-05-01" }, b: "junk", c: { misses: -4, run: "x", last: "yesterday" } } });
    expect(n.weak.a).toEqual({ misses: 2, run: 1, last: "2026-05-01" });
    expect(n.weak.b).toBeUndefined();
    expect(n.weak.c).toEqual({ misses: 0, run: 0, last: "" });
  });
});
