import { addDays } from "../warmup/schedule";
import { activityOn } from "./activity";
import type { Activity } from "./activity";
import type { Progress } from "./model";
import { daysBetween } from "./streak";

// The numbers behind the learning stats page. Everything is derived from the saved progress; nothing is stored just for display.

export interface Summary {
  completed: number;
  /** Lessons whose pass was recorded with the number of test runs it took. */
  measured: number;
  /** Share (0 to 1) of measured lessons passed on the first test run, or null with nothing measured. */
  firstTryRate: number | null;
  /** Median number of test runs to the first pass, or null. */
  medianTries: number | null;
  /** All the time spent on lessons, in milliseconds. */
  totalMs: number;
  streak: { current: number; longest: number };
}

const median = (xs: number[]): number | null => {
  if (xs.length === 0) return null;
  const s = [...xs].sort((a, b) => a - b);
  const m = Math.floor(s.length / 2);
  return s.length % 2 ? (s[m] as number) : ((s[m - 1] as number) + (s[m] as number)) / 2;
};

export function summarize(p: Progress): Summary {
  const done = Object.values(p.lessons).filter((l) => l.status === "complete");
  const tries = done.map((l) => l.runsToFirstPass).filter((n): n is number => n !== undefined && n > 0);
  return {
    completed: done.length,
    measured: tries.length,
    firstTryRate: tries.length ? tries.filter((n) => n === 1).length / tries.length : null,
    medianTries: median(tries),
    totalMs: Object.values(p.lessons).reduce((s, l) => s + l.timeMs, 0),
    streak: { current: p.streak.current, longest: p.streak.longest },
  };
}

export interface Effort {
  lessonId: string;
  tries: number | null;
  ms: number | null;
  hints: number;
  revealed: boolean;
  score: number;
}

/** The completed lessons that took the most effort: many test runs, a long time, hints, the solution. Worst first. */
export function effortRanking(p: Progress, limit = 10): Effort[] {
  const out: Effort[] = [];
  for (const [lessonId, l] of Object.entries(p.lessons)) {
    if (l.status !== "complete") continue;
    const tries = l.runsToFirstPass ?? null;
    const ms = l.msToFirstPass !== undefined && l.msToFirstPass > 0 ? l.msToFirstPass : null;
    const score = (tries ?? 1) - 1 + (ms ?? 0) / 60_000 / 5 + l.hintsUsed + (l.solutionRevealed ? 3 : 0);
    if (score >= 1) out.push({ lessonId, tries, ms, hints: l.hintsUsed, revealed: l.solutionRevealed, score });
  }
  return out.sort((a, b) => b.score - a.score || a.lessonId.localeCompare(b.lessonId)).slice(0, limit);
}

export interface ErrorKind {
  key: string;
  kind: "compile" | "runtime";
  /** The rule id (compile) or the sanitizer's one-line summary (runtime). */
  id: string;
  count: number;
  lastAt: string;
}

/** The problems hit most often, most frequent first. */
export function topErrors(p: Progress, limit = 8): ErrorKind[] {
  return Object.entries(p.errors)
    .map(([key, v]) => ({ key, kind: key.startsWith("runtime:") ? ("runtime" as const) : ("compile" as const), id: key.slice(key.indexOf(":") + 1), count: v.count, lastAt: v.lastAt }))
    .sort((a, b) => b.count - a.count || b.lastAt.localeCompare(a.lastAt) || a.key.localeCompare(b.key))
    .slice(0, limit);
}

export interface TopicAccuracy {
  topic: string;
  seen: number;
  correct: number;
  rate: number;
}

/** Warm-up accuracy per topic, the weakest first. `topicOf` names the topic of a card (cards that no longer exist are skipped). */
export function accuracyByTopic(p: Progress, topicOf: (cardId: string) => string | undefined): TopicAccuracy[] {
  const acc = new Map<string, { seen: number; correct: number }>();
  for (const [id, r] of Object.entries(p.reviews)) {
    const topic = topicOf(id);
    if (!topic || r.seen === 0) continue;
    const cur = acc.get(topic) ?? { seen: 0, correct: 0 };
    acc.set(topic, { seen: cur.seen + r.seen, correct: cur.correct + r.correct });
  }
  return [...acc].map(([topic, v]) => ({ topic, ...v, rate: v.correct / v.seen })).sort((a, b) => a.rate - b.rate || b.seen - a.seen || a.topic.localeCompare(b.topic));
}

const sum = (a: Activity, b: Activity): Activity => ({ warmups: a.warmups + b.warmups, resolves: a.resolves + b.resolves, lessons: a.lessons + b.lessons, runs: a.runs + b.runs, ms: a.ms + b.ms });
const none: Activity = { warmups: 0, resolves: 0, lessons: 0, runs: 0, ms: 0 };

/** Totals for the 7 days ending `today`, and for the 7 days before those. */
export function weekly(p: Progress, today: string): { thisWeek: Activity; lastWeek: Activity } {
  let thisWeek = none;
  let lastWeek = none;
  for (let i = 0; i < 7; i++) {
    thisWeek = sum(thisWeek, activityOn(p, addDays(today, -i)));
    lastWeek = sum(lastWeek, activityOn(p, addDays(today, -7 - i)));
  }
  return { thisWeek, lastWeek };
}

/** How much a day counts in the activity view. */
export const dayPoints = (a: Activity): number => a.runs + a.warmups + a.resolves * 2 + a.lessons * 5;

/** 0 (nothing) to 4 (a big day). */
export const level = (points: number): 0 | 1 | 2 | 3 | 4 => (points <= 0 ? 0 : points < 5 ? 1 : points < 15 ? 2 : points < 30 ? 3 : 4);

export interface HeatDay {
  date: string;
  points: number;
  level: 0 | 1 | 2 | 3 | 4;
  /** Plain-language summary of the day. */
  text: string;
  /** After today (drawn empty). */
  future: boolean;
}

/**
 * The last `weeks` weeks as columns of 7 days (Sunday on top), the last column ending with the week of `today`.
 * `weekday` is the day of the week of `today` (0 = Sunday).
 */
export function heatmap(p: Progress, today: string, weekday: number, weeks = 12): HeatDay[][] {
  const lastSunday = addDays(today, -weekday);
  const cols: HeatDay[][] = [];
  for (let w = weeks - 1; w >= 0; w--) {
    const col: HeatDay[] = [];
    for (let d = 0; d < 7; d++) {
      const date = addDays(lastSunday, -7 * w + d);
      const future = daysBetween(date, today) < 0;
      const a = activityOn(p, date);
      const points = future ? 0 : dayPoints(a);
      const parts = [a.runs && `${a.runs} run${a.runs === 1 ? "" : "s"}`, a.warmups && `${a.warmups} card${a.warmups === 1 ? "" : "s"}`, a.resolves && `${a.resolves} re-solve${a.resolves === 1 ? "" : "s"}`, a.lessons && `${a.lessons} lesson${a.lessons === 1 ? "" : "s"} completed`].filter(Boolean);
      col.push({ date, points, level: level(points), text: future ? "" : parts.length ? parts.join(", ") : "no activity", future });
    }
    cols.push(col);
  }
  return cols;
}

/** "1 h 05 min", "12 min", "45 s". */
export function formatDuration(ms: number): string {
  const s = Math.round(ms / 1000);
  if (s < 60) return `${s} s`;
  const m = Math.round(s / 60);
  if (m < 60) return `${m} min`;
  return `${Math.floor(m / 60)} h ${String(m % 60).padStart(2, "0")} min`;
}
