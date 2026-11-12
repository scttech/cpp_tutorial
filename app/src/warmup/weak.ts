import { daysBetween } from "../state/streak";
import type { Card } from "./cards";
import { hashString } from "./rand";
import { overdueDays } from "./schedule";
import type { Review } from "./schedule";
import { eligible, SESSION_SIZE, topicKey } from "./session";

// Weak spots: the lessons and general topics you keep getting wrong. A weak spot is a "unit": one lesson (its gotcha cards)
// or one general-topic group of the CS bank. Warm-up answers keep a small tally per unit; lesson friction (hints, the revealed
// solution, many failed runs) adds to a lesson's score. A weak spot clears itself after N right answers in a row (a setting).

export interface WeakEntry {
  misses: number;
  /** Right answers in a row since the last miss (or since the tally began). */
  run: number;
  /** Local date of the last miss, "" when there has been none. */
  last: string;
}

export const DEFAULT_CLEAR = 5;
export const CLEAR_RANGE = { min: 1, max: 20 } as const;

export const unitOf = (c: Card): string => (c.source === "demo" ? `lesson:${c.lessonId}` : `topic:${c.topic}`);

/** The tally after one answered card of `unit`. */
export function recordAnswer(weak: Record<string, WeakEntry>, unit: string, correct: boolean, today: string): Record<string, WeakEntry> {
  const prev = weak[unit] ?? { misses: 0, run: 0, last: "" };
  return { ...weak, [unit]: correct ? { ...prev, run: prev.run + 1 } : { misses: prev.misses + 1, run: 0, last: today } };
}

/** The parts of a lesson's progress that show it was hard going. */
export interface Friction {
  hintsUsed: number;
  solutionRevealed: boolean;
  failedRuns: number;
  aiHelpUsed: number;
}

/** Points and plain-language reasons for how hard a lesson was. */
export function frictionOf(f: Friction): { points: number; reasons: string[] } {
  const reasons: string[] = [];
  let points = 0;
  if (f.solutionRevealed) {
    points += 2;
    reasons.push("you looked at the solution");
  }
  if (f.hintsUsed >= 2) {
    points += 1;
    reasons.push(`${f.hintsUsed} hints used`);
  }
  if (f.failedRuns >= 5) {
    points += 1;
    reasons.push(`${f.failedRuns} failed test runs before it passed`);
  }
  if (f.aiHelpUsed >= 3) {
    points += 1;
    reasons.push("a lot of tutor help");
  }
  return { points, reasons };
}

export interface WeakSpot {
  unit: string;
  kind: "lesson" | "topic";
  /** The lesson id for a lesson unit. */
  lessonId?: string;
  title: string;
  score: number;
  reasons: string[];
  /** Right answers in a row so far, and how many clear it. */
  run: number;
  need: number;
  cards: number;
}

export interface WeakInput {
  cards: Card[];
  weak: Record<string, WeakEntry>;
  /** Friction of each completed lesson, by id. */
  friction: Record<string, Friction>;
  completed: ReadonlySet<string>;
  titleOf(lessonId: string): string;
  clearAfter: number;
  today: string;
}

/** Days since the last miss counts as "recent" up to a week. */
const RECENT_DAYS = 7;

/**
 * The current weak spots, worst first. A unit needs cards to practise with, a reason to be called weak (a miss, or a lesson that was
 * hard going), and fewer than `clearAfter` right answers in a row since.
 */
export function weakSpots(input: WeakInput): WeakSpot[] {
  const units = new Map<string, { card: Card; n: number }>();
  for (const c of eligible(input.cards, input.completed)) {
    const u = unitOf(c);
    const e = units.get(u);
    if (e) e.n += 1;
    else units.set(u, { card: c, n: 1 });
  }
  // A completed lesson with no warm-up cards can still be weak through failed re-solves; it is practised by re-solving it.
  const cardless: [string, { card: Card | undefined; n: number }][] = [];
  for (const unit of Object.keys(input.weak)) {
    const id = unit.startsWith("lesson:") ? unit.slice("lesson:".length) : "";
    if (id && !units.has(unit) && input.completed.has(id)) cardless.push([unit, { card: undefined, n: 0 }]);
  }
  const out: WeakSpot[] = [];
  for (const [unit, { card, n }] of [...units, ...cardless]) {
    const entry = input.weak[unit] ?? { misses: 0, run: 0, last: "" };
    const lessonId = card ? (card.source === "demo" ? card.lessonId : undefined) : unit.slice("lesson:".length);
    const fr = lessonId && input.friction[lessonId] ? frictionOf(input.friction[lessonId] as Friction) : { points: 0, reasons: [] as string[] };
    const reasons: string[] = [];
    let score = 0;
    if (entry.misses > 0) {
      score += Math.min(entry.misses, 6) * 2;
      const ago = entry.last ? daysBetween(entry.last, input.today) : 99;
      if (ago <= RECENT_DAYS) score += 2;
      reasons.push(`missed ${entry.misses} time${entry.misses === 1 ? "" : "s"}${ago <= RECENT_DAYS ? (ago <= 0 ? ", today" : `, ${ago} day${ago === 1 ? "" : "s"} ago`) : ""}`);
    }
    if (fr.points >= 2 || (fr.points > 0 && entry.misses > 0)) {
      score += fr.points;
      reasons.push(...fr.reasons);
    }
    if (score === 0 || entry.run >= input.clearAfter) continue;
    out.push({
      unit,
      kind: lessonId ? "lesson" : "topic",
      ...(lessonId ? { lessonId } : {}),
      title: lessonId ? input.titleOf(lessonId) : (card as Card).topic,
      score,
      reasons,
      run: entry.run,
      need: input.clearAfter,
      cards: n,
    });
  }
  return out.sort((a, b) => b.score - a.score || a.unit.localeCompare(b.unit));
}

export interface PracticeInput {
  cards: Card[];
  reviews: Record<string, Review>;
  completed: ReadonlySet<string>;
  /** Units to practise (from the weak-spot list). */
  units: ReadonlySet<string>;
  today: string;
  size?: number;
}

/**
 * A focused set from the weak units, ignoring due dates: cards you got wrong before first, then ones never asked, then the rest
 * (lowest box first). One card per gotcha. The same inputs give the same set.
 */
export function pickPractice(input: PracticeInput): Card[] {
  const size = input.size ?? SESSION_SIZE;
  const rank = (c: Card): number => {
    const r = input.reviews[c.id];
    if (!r) return 1;
    return r.correct < r.seen ? 0 : 2 + r.box;
  };
  const pool = eligible(input.cards, input.completed)
    .filter((c) => input.units.has(unitOf(c)))
    .sort((a, b) => {
      const ra = input.reviews[a.id];
      const rb = input.reviews[b.id];
      return rank(a) - rank(b) || (rb ? overdueDays(rb, input.today) : 0) - (ra ? overdueDays(ra, input.today) : 0) || hashString(a.id) - hashString(b.id);
    });
  const out: Card[] = [];
  const seen = new Set<string>();
  for (const c of pool) {
    const key = topicKey(c);
    if (seen.has(key)) continue;
    seen.add(key);
    out.push(c);
    if (out.length === size) break;
  }
  return out;
}
