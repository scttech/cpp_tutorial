import type { Tier } from "../runner/types";
import { MAX_BOX } from "../warmup/schedule";
import type { Review } from "../warmup/schedule";
import { CLEAR_RANGE, DEFAULT_CLEAR } from "../warmup/weak";
import type { WeakEntry } from "../warmup/weak";
import type { Activity } from "./activity";
import type { DailyPlan } from "./plan";
import type { ResolveEntry } from "./resolves";

// The persisted progress model (spec section 2.4). Every field added after the first release has a
// default in normalize(), so older saved data and imported files keep working.

export interface LessonProgress {
  status: "available" | "in_progress" | "complete";
  bestTiers: Record<Tier, boolean>;
  attempts: number; // Run + Run Tests presses
  testRuns: number;
  failedRuns: number; // test runs that did not fully pass, counted until the lesson is first completed
  runsToFirstPass?: number; // test runs it took to complete the lesson the first time
  hintsUsed: number;
  aiHelpUsed: number;
  explainUsed: boolean; // asked the tutor to explain or discuss (for the Good Question badge)
  bestLines?: number; // fewest code lines of a passing version (for the Refactorer badge)
  lastWarnings?: number; // compiler warnings in the latest successful build (for the Wall of Warnings badge)
  solutionRevealed: boolean;
  lastCode: Record<string, string>;
  firstCompleteAt?: string; // ISO
  /** Time spent on the lesson page (open, visible and in use), in milliseconds. */
  timeMs: number;
  /** timeMs at the moment the lesson was first completed. */
  msToFirstPass?: number;
}

export interface Streak {
  current: number;
  longest: number;
  lastActiveDate: string | null; // local date, YYYY-MM-DD
  forgivenOn: string | null; // the day a missed day was last forgiven (one forgiveness per 7 days)
}

export interface EarnedBadge {
  earnedAt: string; // ISO
  evidence?: { lessons?: string[] };
}

export interface Settings {
  pace: "full" | "fast";
  /** A weak spot clears after this many right answers in a row. */
  weakClear: number;
}

export interface Progress {
  version: 1;
  lessons: Record<string, LessonProgress>;
  streak: Streak;
  badges: Record<string, EarnedBadge>;
  events: Record<string, { count: number; lastAt: string }>;
  /** Warm-up cards: when each one that has been asked is due again (see warmup/schedule.ts). */
  reviews: Record<string, Review>;
  /** Per lesson or topic: misses and the current run of right answers (see warmup/weak.ts). */
  weak: Record<string, WeakEntry>;
  /** Re-solve schedule per completed lesson (see state/resolves.ts). A lesson without an entry is due 3 days after it was first completed. */
  resolves: Record<string, ResolveEntry>;
  /** What was done on each recent day (see state/activity.ts), and the plan chosen for the current day (state/plan.ts). */
  activity: Record<string, Activity>;
  plan: DailyPlan | null;
  /** How often each kind of problem was hit: "compile:<rule id>" (or "compile:other") and "runtime:<sanitizer summary>". */
  errors: Record<string, { count: number; lastAt: string }>;
  settings: Settings;
}

export const emptyLesson = (): LessonProgress => ({
  status: "available",
  bestTiers: { works: false, robust: false, fast: false, clean: false },
  attempts: 0,
  testRuns: 0,
  failedRuns: 0,
  hintsUsed: 0,
  aiHelpUsed: 0,
  explainUsed: false,
  solutionRevealed: false,
  lastCode: {},
  timeMs: 0,
});

export const emptyProgress = (): Progress => ({
  version: 1,
  lessons: {},
  streak: { current: 0, longest: 0, lastActiveDate: null, forgivenOn: null },
  badges: {},
  events: {},
  reviews: {},
  weak: {},
  resolves: {},
  activity: {},
  plan: null,
  errors: {},
  settings: { pace: "full", weakClear: DEFAULT_CLEAR },
});

const isObj = (v: unknown): v is Record<string, unknown> => typeof v === "object" && v !== null && !Array.isArray(v);
const num = (v: unknown, d = 0) => (typeof v === "number" && Number.isFinite(v) && v >= 0 ? Math.floor(v) : d);
const str = (v: unknown): string | undefined => (typeof v === "string" ? v : undefined);

function normalizeLesson(raw: unknown): LessonProgress {
  const base = emptyLesson();
  if (!isObj(raw)) return base;
  const tiers = isObj(raw.bestTiers) ? raw.bestTiers : {};
  const code: Record<string, string> = {};
  if (isObj(raw.lastCode)) for (const [k, v] of Object.entries(raw.lastCode)) if (typeof v === "string") code[k] = v;
  const status = raw.status === "complete" || raw.status === "in_progress" ? raw.status : "available";
  const out: LessonProgress = {
    status,
    bestTiers: {
      works: tiers.works === true,
      robust: tiers.robust === true,
      fast: tiers.fast === true,
      clean: tiers.clean === true,
    },
    attempts: num(raw.attempts),
    testRuns: num(raw.testRuns),
    failedRuns: num(raw.failedRuns),
    hintsUsed: num(raw.hintsUsed),
    aiHelpUsed: num(raw.aiHelpUsed),
    explainUsed: raw.explainUsed === true,
    solutionRevealed: raw.solutionRevealed === true,
    lastCode: code,
    timeMs: num(raw.timeMs),
  };
  if (typeof raw.runsToFirstPass === "number") out.runsToFirstPass = num(raw.runsToFirstPass);
  if (typeof raw.bestLines === "number") out.bestLines = num(raw.bestLines);
  if (typeof raw.lastWarnings === "number") out.lastWarnings = num(raw.lastWarnings);
  if (typeof raw.msToFirstPass === "number") out.msToFirstPass = num(raw.msToFirstPass);
  const done = str(raw.firstCompleteAt);
  if (done) out.firstCompleteAt = done;
  return out;
}

/**
 * Coerce untrusted data (saved storage, an imported file) into a valid Progress.
 * Unknown fields are dropped and bad values fall back to defaults; it never throws.
 */
export function normalize(raw: unknown): Progress {
  const out = emptyProgress();
  if (!isObj(raw)) return out;
  if (isObj(raw.lessons)) {
    for (const [id, v] of Object.entries(raw.lessons)) out.lessons[id] = normalizeLesson(v);
  }
  if (isObj(raw.streak)) {
    out.streak = {
      current: num(raw.streak.current),
      longest: num(raw.streak.longest),
      lastActiveDate: str(raw.streak.lastActiveDate) ?? null,
      forgivenOn: str(raw.streak.forgivenOn) ?? null,
    };
  }
  if (isObj(raw.badges)) {
    for (const [id, v] of Object.entries(raw.badges)) {
      if (!isObj(v) || typeof v.earnedAt !== "string") continue;
      const lessons = isObj(v.evidence) && Array.isArray(v.evidence.lessons) ? v.evidence.lessons.filter((x): x is string => typeof x === "string") : undefined;
      out.badges[id] = { earnedAt: v.earnedAt, ...(lessons ? { evidence: { lessons } } : {}) };
    }
  }
  if (isObj(raw.events)) {
    for (const [id, v] of Object.entries(raw.events)) {
      if (isObj(v)) out.events[id] = { count: num(v.count), lastAt: str(v.lastAt) ?? "" };
    }
  }
  if (isObj(raw.reviews)) {
    for (const [id, v] of Object.entries(raw.reviews)) {
      if (!isObj(v) || typeof v.due !== "string" || !/^\d{4}-\d{2}-\d{2}$/.test(v.due)) continue;
      out.reviews[id] = { box: Math.min(MAX_BOX, Math.max(1, num(v.box, 1))), due: v.due, seen: num(v.seen), correct: num(v.correct) };
    }
  }
  if (isObj(raw.weak)) {
    for (const [unit, v] of Object.entries(raw.weak)) {
      if (isObj(v)) out.weak[unit] = { misses: num(v.misses), run: num(v.run), last: typeof v.last === "string" && /^(\d{4}-\d{2}-\d{2})?$/.test(v.last) ? v.last : "" };
    }
  }
  if (isObj(raw.resolves)) {
    for (const [id, v] of Object.entries(raw.resolves)) {
      if (!isObj(v) || typeof v.due !== "string" || !/^\d{4}-\d{2}-\d{2}$/.test(v.due)) continue;
      out.resolves[id] = {
        box: Math.min(4, Math.max(1, num(v.box, 1))),
        due: v.due,
        count: num(v.count),
        misses: num(v.misses),
        lastAt: str(v.lastAt) ?? "",
        ...(typeof v.bestMs === "number" ? { bestMs: num(v.bestMs) } : {}),
      };
    }
  }
  if (isObj(raw.activity)) {
    for (const [date, v] of Object.entries(raw.activity)) {
      if (isObj(v) && /^\d{4}-\d{2}-\d{2}$/.test(date)) out.activity[date] = { warmups: num(v.warmups), resolves: num(v.resolves), lessons: num(v.lessons), runs: num(v.runs), ms: num(v.ms) };
    }
  }
  if (isObj(raw.plan) && typeof raw.plan.date === "string" && /^\d{4}-\d{2}-\d{2}$/.test(raw.plan.date)) {
    out.plan = { date: raw.plan.date, ...(str(raw.plan.lessonId) ? { lessonId: str(raw.plan.lessonId) as string } : {}), ...(str(raw.plan.resolveId) ? { resolveId: str(raw.plan.resolveId) as string } : {}) };
  }
  if (isObj(raw.errors)) {
    for (const [key, v] of Object.entries(raw.errors)) {
      if (isObj(v) && /^(compile|runtime):.+/.test(key)) out.errors[key] = { count: num(v.count), lastAt: str(v.lastAt) ?? "" };
    }
  }
  if (isObj(raw.settings)) {
    if (raw.settings.pace === "fast" || raw.settings.pace === "full") out.settings.pace = raw.settings.pace;
    if (typeof raw.settings.weakClear === "number") out.settings.weakClear = Math.min(CLEAR_RANGE.max, Math.max(CLEAR_RANGE.min, num(raw.settings.weakClear, DEFAULT_CLEAR)));
  }
  return out;
}
