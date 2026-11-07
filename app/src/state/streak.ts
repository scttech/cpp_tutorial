import type { Streak } from "./model";

/** Local calendar date as YYYY-MM-DD (the learner's own day, not UTC). */
export function localDate(d: Date): string {
  const p = (n: number) => String(n).padStart(2, "0");
  return `${d.getFullYear()}-${p(d.getMonth() + 1)}-${p(d.getDate())}`;
}

/** Whole days from a to b (both YYYY-MM-DD). Computed in UTC so daylight-saving shifts do not matter. */
export function daysBetween(a: string, b: string): number {
  const [ya, ma, da] = a.split("-").map(Number) as [number, number, number];
  const [yb, mb, db] = b.split("-").map(Number) as [number, number, number];
  return Math.round((Date.UTC(yb, mb - 1, db) - Date.UTC(ya, ma - 1, da)) / 86_400_000);
}

/**
 * Record activity on `today`. Streaks are deliberately low-pressure: ONE missed day is forgiven, at
 * most once per 7 days. Missing two or more days in a row (or using up the forgiveness) resets to 1.
 */
export function touchStreak(s: Streak, today: string): Streak {
  if (s.lastActiveDate === today) return s;
  if (s.lastActiveDate === null) return { current: 1, longest: Math.max(1, s.longest), lastActiveDate: today, forgivenOn: s.forgivenOn };
  const gap = daysBetween(s.lastActiveDate, today);
  let current: number;
  let forgivenOn = s.forgivenOn;
  if (gap < 0) {
    return s; // clock went backwards: ignore rather than corrupt the streak
  } else if (gap === 1) {
    current = s.current + 1;
  } else if (gap === 2 && (forgivenOn === null || daysBetween(forgivenOn, today) >= 7)) {
    current = s.current + 1;
    forgivenOn = today;
  } else {
    current = 1;
  }
  return { current, longest: Math.max(s.longest, current), lastActiveDate: today, forgivenOn };
}
