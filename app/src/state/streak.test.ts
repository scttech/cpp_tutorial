import { describe, expect, it } from "vitest";
import type { Streak } from "./model";
import { daysBetween, localDate, touchStreak } from "./streak";

const fresh = (): Streak => ({ current: 0, longest: 0, lastActiveDate: null, forgivenOn: null });
const run = (days: string[], s = fresh()) => days.reduce((acc, d) => touchStreak(acc, d), s);

describe("daysBetween / localDate", () => {
  it("counts whole days, across month and year boundaries", () => {
    expect(daysBetween("2026-03-01", "2026-03-02")).toBe(1);
    expect(daysBetween("2026-01-31", "2026-02-01")).toBe(1);
    expect(daysBetween("2025-12-31", "2026-01-01")).toBe(1);
    expect(daysBetween("2024-02-28", "2024-03-01")).toBe(2); // leap year
    expect(daysBetween("2026-05-05", "2026-05-05")).toBe(0);
    expect(daysBetween("2026-05-06", "2026-05-05")).toBe(-1);
  });
  it("is not affected by daylight-saving changes", () => {
    expect(daysBetween("2026-03-07", "2026-03-09")).toBe(2); // US spring-forward weekend
    expect(daysBetween("2026-10-31", "2026-11-02")).toBe(2); // fall-back weekend
  });
  it("formats the local calendar date with zero padding", () => {
    expect(localDate(new Date(2026, 0, 5, 23, 59))).toBe("2026-01-05");
  });
});

describe("touchStreak", () => {
  it("starts at 1 and ignores a second run on the same day", () => {
    const s = touchStreak(fresh(), "2026-05-01");
    expect(s).toMatchObject({ current: 1, longest: 1, lastActiveDate: "2026-05-01" });
    expect(touchStreak(s, "2026-05-01")).toBe(s);
  });
  it("increments on consecutive days", () => {
    expect(run(["2026-05-01", "2026-05-02", "2026-05-03"])).toMatchObject({ current: 3, longest: 3 });
  });
  it("forgives ONE missed day", () => {
    const s = run(["2026-05-01", "2026-05-02", "2026-05-04"]); // missed the 3rd
    expect(s).toMatchObject({ current: 3, forgivenOn: "2026-05-04" });
  });
  it("does not forgive a second missed day within a week", () => {
    const s = run(["2026-05-01", "2026-05-03", "2026-05-05"]); // missed the 2nd and the 4th
    expect(s.current).toBe(1);
    expect(s.longest).toBe(2);
  });
  it("forgives again once a week has passed since the last forgiveness", () => {
    const s = run(["2026-05-01", "2026-05-03", "2026-05-04", "2026-05-05", "2026-05-06", "2026-05-07", "2026-05-08", "2026-05-09", "2026-05-11"]);
    expect(s.current).toBe(9); // forgiven on the 3rd, and again on the 11th (8 days later)
  });
  it("resets after two or more missed days and keeps the longest streak", () => {
    const s = run(["2026-05-01", "2026-05-02", "2026-05-03", "2026-05-07"]);
    expect(s).toMatchObject({ current: 1, longest: 3 });
  });
  it("handles month boundaries", () => {
    expect(run(["2026-01-30", "2026-01-31", "2026-02-01"]).current).toBe(3);
  });
  it("ignores a clock that moved backwards", () => {
    const s = run(["2026-05-05", "2026-05-06"]);
    expect(touchStreak(s, "2026-05-01")).toBe(s);
  });
});
