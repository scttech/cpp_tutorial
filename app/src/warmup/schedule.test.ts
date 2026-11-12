import { describe, expect, it } from "vitest";
import { addDays, answer, INTERVALS, isDue, MAX_BOX, overdueDays } from "./schedule";

describe("addDays", () => {
  it("crosses month, year and leap-day boundaries", () => {
    expect(addDays("2026-01-30", 3)).toBe("2026-02-02");
    expect(addDays("2026-12-31", 1)).toBe("2027-01-01");
    expect(addDays("2028-02-28", 1)).toBe("2028-02-29");
    expect(addDays("2026-03-08", 1)).toBe("2026-03-09"); // a daylight-saving date
    expect(addDays("2026-05-10", 0)).toBe("2026-05-10");
  });
});

describe("answer", () => {
  const today = "2026-05-10";
  it("a first right answer starts in box 2 and is due in 3 days", () => {
    expect(answer(undefined, true, today)).toEqual({ box: 2, due: "2026-05-13", seen: 1, correct: 1 });
  });
  it("a first wrong answer starts in box 1 and is due tomorrow", () => {
    expect(answer(undefined, false, today)).toEqual({ box: 1, due: "2026-05-11", seen: 1, correct: 0 });
  });
  it("each right answer moves up a box, to the top and no further", () => {
    let r = answer(undefined, true, today);
    const boxes = [r.box];
    for (let i = 0; i < 6; i++) {
      r = answer(r, true, today);
      boxes.push(r.box);
    }
    expect(boxes).toEqual([2, 3, 4, 5, 5, 5, 5]);
    expect(r.due).toBe(addDays(today, INTERVALS[MAX_BOX - 1]));
    expect(r.seen).toBe(7);
  });
  it("a wrong answer sends any card back to box 1", () => {
    const high = { box: 5, due: today, seen: 6, correct: 6 };
    expect(answer(high, false, today)).toEqual({ box: 1, due: "2026-05-11", seen: 7, correct: 6 });
  });
  it("does not change the card it was given", () => {
    const prev = { box: 2, due: today, seen: 1, correct: 1 };
    answer(prev, true, today);
    expect(prev).toEqual({ box: 2, due: today, seen: 1, correct: 1 });
  });
});

describe("isDue", () => {
  const r = { box: 2, due: "2026-05-10", seen: 1, correct: 1 };
  it("is due on the due date and after, not before", () => {
    expect(isDue(r, "2026-05-09")).toBe(false);
    expect(isDue(r, "2026-05-10")).toBe(true);
    expect(isDue(r, "2026-06-01")).toBe(true);
    expect(overdueDays(r, "2026-05-12")).toBe(2);
    expect(overdueDays(r, "2026-05-08")).toBe(-2);
  });
});
