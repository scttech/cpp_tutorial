import type { RunResult } from "../runner/types";
import { addDays } from "../warmup/schedule";
import { recordAnswer } from "../warmup/weak";
import type { Progress } from "./model";
import { bumpEvent } from "./recorder";
import { daysBetween, localDate, touchStreak } from "./streak";

// Re-solving: a lesson you have passed comes back after 3, 7, 14 and then every 30 days. You solve it again from the starter code (your saved
// solution is never touched). A clean re-solve moves it to the next interval; a miss (a hint, the solution, or giving up) sends it back to 3 days.

/** Days until the next re-solve for a lesson now in box 1, 2, 3 or 4. */
export const RESOLVE_INTERVALS = [3, 7, 14, 30] as const;
export const MAX_RESOLVE_BOX = RESOLVE_INTERVALS.length;

export interface ResolveEntry {
  box: number;
  /** Local date (YYYY-MM-DD) from which a re-solve is due. */
  due: string;
  /** Re-solves finished, and how many of them were misses. */
  count: number;
  misses: number;
  lastAt: string;
  /** Fastest clean re-solve, in milliseconds. */
  bestMs?: number;
}

export interface ResolveOutcome {
  /** Passed all the tests. */
  passed: boolean;
  hintsUsed: number;
  solutionRevealed: boolean;
  /** Time from the start of the re-solve, in milliseconds. */
  ms: number;
}

/** Every test passed and the program built and finished in time. */
export const passedAll = (r: RunResult): boolean => r.compiled && !r.timedOut && r.tests.length > 0 && r.tests.every((t) => t.status === "pass");

/** A clean re-solve: passed with no hint and without looking at the solution. */
export const isClean = (o: ResolveOutcome): boolean => o.passed && o.hintsUsed === 0 && !o.solutionRevealed;

/**
 * The re-solve state of a lesson. A lesson passed before re-solving existed has no entry yet: it is due 3 days after it was first completed.
 * Null for a lesson that is not complete.
 */
export function resolveFor(p: Progress, lessonId: string): ResolveEntry | null {
  const lesson = p.lessons[lessonId];
  if (!lesson || lesson.status !== "complete") return null;
  const saved = p.resolves[lessonId];
  if (saved) return saved;
  const done = lesson.firstCompleteAt ? localDate(new Date(lesson.firstCompleteAt)) : localDate(new Date());
  return { box: 1, due: addDays(done, RESOLVE_INTERVALS[0]), count: 0, misses: 0, lastAt: "" };
}

export const isResolveDue = (e: ResolveEntry, today: string): boolean => daysBetween(e.due, today) >= 0;

/** Lessons due for a re-solve, the most overdue first. */
export function dueResolves(p: Progress, today: string): { lessonId: string; entry: ResolveEntry; overdue: number }[] {
  const out: { lessonId: string; entry: ResolveEntry; overdue: number }[] = [];
  for (const id of Object.keys(p.lessons)) {
    const entry = resolveFor(p, id);
    if (entry && isResolveDue(entry, today)) out.push({ lessonId: id, entry, overdue: daysBetween(entry.due, today) });
  }
  return out.sort((a, b) => b.overdue - a.overdue || a.lessonId.localeCompare(b.lessonId));
}

/**
 * Fold a finished re-solve into the progress: the lesson's next due date, the weak-spot tally (a clean re-solve is a right answer for the lesson,
 * a miss is a wrong one), the streak, and the "resolve" counters. Pure and deterministic given `now`.
 */
export function applyResolve(state: Progress, lessonId: string, outcome: ResolveOutcome, now: Date): Progress {
  const prev = resolveFor(state, lessonId);
  if (!prev) return state;
  const today = localDate(now);
  const iso = now.toISOString();
  const clean = isClean(outcome);
  const box = clean ? Math.min(MAX_RESOLVE_BOX, prev.box + 1) : 1;
  const entry: ResolveEntry = {
    box,
    due: addDays(today, RESOLVE_INTERVALS[box - 1] as number),
    count: prev.count + 1,
    misses: prev.misses + (clean ? 0 : 1),
    lastAt: iso,
    ...(clean ? { bestMs: prev.bestMs === undefined ? outcome.ms : Math.min(prev.bestMs, outcome.ms) } : prev.bestMs !== undefined ? { bestMs: prev.bestMs } : {}),
  };
  let next: Progress = { ...state, resolves: { ...state.resolves, [lessonId]: entry }, weak: recordAnswer(state.weak, `lesson:${lessonId}`, clean, today) };
  next = bumpEvent(next, "resolve-done", iso);
  if (clean) next = bumpEvent(next, "resolve-clean", iso);
  return { ...next, streak: touchStreak(next.streak, today) };
}

/** "3 days", "a week", ... for the interval after a re-solve. */
export const describeInterval = (days: number): string => (days === 1 ? "tomorrow" : `${days} days`);
