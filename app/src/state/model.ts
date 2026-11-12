import type { Tier } from "../runner/types";

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
  solutionRevealed: boolean;
  lastCode: Record<string, string>;
  firstCompleteAt?: string; // ISO
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
}

export interface Progress {
  version: 1;
  lessons: Record<string, LessonProgress>;
  streak: Streak;
  badges: Record<string, EarnedBadge>;
  events: Record<string, { count: number; lastAt: string }>;
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
});

export const emptyProgress = (): Progress => ({
  version: 1,
  lessons: {},
  streak: { current: 0, longest: 0, lastActiveDate: null, forgivenOn: null },
  badges: {},
  events: {},
  settings: { pace: "full" },
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
  };
  if (typeof raw.runsToFirstPass === "number") out.runsToFirstPass = num(raw.runsToFirstPass);
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
  if (isObj(raw.settings) && (raw.settings.pace === "fast" || raw.settings.pace === "full")) {
    out.settings.pace = raw.settings.pace;
  }
  return out;
}
