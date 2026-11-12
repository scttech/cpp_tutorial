import { addDays } from "../warmup/schedule";
import type { Progress } from "./model";

/** What was done on one local day. Kept for the last KEEP_DAYS days (today's plan reads it; the learning stats draw from it). */
export interface Activity {
  /** Warm-up cards answered. */
  warmups: number;
  /** Re-solves finished. */
  resolves: number;
  /** Lessons completed for the first time. */
  lessons: number;
  /** Runs and test runs pressed. */
  runs: number;
  /** Milliseconds spent working on lessons (the page open and in use). */
  ms: number;
}

export const KEEP_DAYS = 120;
export const emptyActivity = (): Activity => ({ warmups: 0, resolves: 0, lessons: 0, runs: 0, ms: 0 });

/** Add `by` to one counter of `date`, and forget days older than KEEP_DAYS. */
export function bumpActivity(state: Progress, date: string, field: keyof Activity, by = 1): Progress {
  const oldest = addDays(date, -KEEP_DAYS);
  const kept = Object.fromEntries(Object.entries(state.activity).filter(([d]) => d >= oldest));
  const cur = kept[date] ?? emptyActivity();
  return { ...state, activity: { ...kept, [date]: { ...cur, [field]: cur[field] + by } } };
}

export const activityOn = (state: Progress, date: string): Activity => state.activity[date] ?? emptyActivity();
