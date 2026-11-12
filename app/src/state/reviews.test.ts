import { describe, expect, it } from "vitest";
import { evaluate } from "../badges/engine";
import { emptyProgress, normalize } from "./model";
import { applyReview } from "./reviews";

const NOON = new Date(2026, 4, 10, 12, 0, 0); // 2026-05-10, local time

describe("applyReview", () => {
  it("records the card's box and due date, and counts the answer", () => {
    const s = applyReview(emptyProgress(), "q:demorgan", true, NOON);
    expect(s.reviews["q:demorgan"]).toEqual({ box: 2, due: "2026-05-13", seen: 1, correct: 1 });
    expect(s.events["warmup-answered"]?.count).toBe(1);
    expect(s.events["warmup-correct"]?.count).toBe(1);
  });
  it("a wrong answer counts as answered but not as correct", () => {
    const s = applyReview(emptyProgress(), "q:x", false, NOON);
    expect(s.reviews["q:x"]).toMatchObject({ box: 1, due: "2026-05-11" });
    expect(s.events["warmup-answered"]?.count).toBe(1);
    expect(s.events["warmup-correct"]).toBeUndefined();
  });
  it("a warm-up counts as a day of practice for the streak", () => {
    const s = applyReview(emptyProgress(), "q:x", false, NOON);
    expect(s.streak).toMatchObject({ current: 1, lastActiveDate: "2026-05-10" });
    const next = applyReview(s, "q:y", true, new Date(2026, 4, 11, 9, 0, 0));
    expect(next.streak.current).toBe(2);
  });
  it("does not change the state it was given", () => {
    const before = emptyProgress();
    applyReview(before, "q:x", true, NOON);
    expect(before.reviews).toEqual({});
    expect(before.events).toEqual({});
  });
  it("the Warm-Up badges unlock at 5, 25 and 100 correct answers (wrong ones do not count)", () => {
    const rule = (n: number) => ({ type: "custom-event" as const, event: "warmup-correct", atLeast: n });
    let s = emptyProgress();
    const catalog = { lessons: [] };
    for (let i = 0; i < 4; i++) s = applyReview(s, `q:${i}`, true, NOON);
    s = applyReview(s, "q:wrong", false, NOON);
    expect(evaluate(rule(5), s, catalog).met).toBe(false);
    s = applyReview(s, "q:4", true, NOON);
    expect(evaluate(rule(5), s, catalog).met).toBe(true);
    expect(evaluate(rule(25), s, catalog).met).toBe(false);
  });
});

describe("normalize: reviews", () => {
  it("keeps valid entries and defaults the rest", () => {
    const n = normalize({ reviews: { "q:a": { box: 3, due: "2026-06-01", seen: 4, correct: 3 } } });
    expect(n.reviews["q:a"]).toEqual({ box: 3, due: "2026-06-01", seen: 4, correct: 3 });
  });
  it("drops entries with a bad due date and clamps the box", () => {
    const n = normalize({ reviews: { a: { box: 3, due: "tomorrow" }, b: { box: 99, due: "2026-06-01" }, c: { box: -4, due: "2026-06-01" }, d: "x" } });
    expect(Object.keys(n.reviews).sort()).toEqual(["b", "c"]);
    expect(n.reviews.b?.box).toBe(5);
    expect(n.reviews.c?.box).toBe(1);
  });
  it("older saved data without reviews loads with none", () => {
    expect(normalize({ lessons: {} }).reviews).toEqual({});
  });
});
