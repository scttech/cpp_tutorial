import type { Card } from "./cards";
import { hashString, shuffled } from "./rand";
import { isDue, overdueDays } from "./schedule";
import type { Review } from "./schedule";

export const SESSION_SIZE = 5;

export interface PickInput {
  cards: Card[];
  reviews: Record<string, Review>;
  /** Lessons the learner has completed: a gotcha card is only asked for those. */
  completed: ReadonlySet<string>;
  today: string;
  size?: number;
}

/** Cards that may be asked: bank questions always, gotcha cards once their lesson is complete. */
export const eligible = (cards: Card[], completed: ReadonlySet<string>): Card[] =>
  cards.filter((c) => c.source === "cs" || (c.lessonId !== undefined && completed.has(c.lessonId)));

/** How many cards are due for review, and how many have never been asked. */
export function counts({ cards, reviews, completed, today }: PickInput): { due: number; fresh: number } {
  let due = 0;
  let fresh = 0;
  for (const c of eligible(cards, completed)) {
    const r = reviews[c.id];
    if (!r) fresh += 1;
    else if (isDue(r, today)) due += 1;
  }
  return { due, fresh };
}

export const topicKey = (c: Card) => (c.source === "demo" ? `${c.lessonId}:${c.gotchaId}` : c.id);

/**
 * Today's cards: reviews that are due come first (the most overdue first, but never more than 60% of the set, so something new keeps
 * arriving), then cards never asked before, two gotcha cards for each general one. A gotcha is asked at most once per set.
 * The same inputs always give the same set.
 */
export function pickSession(input: PickInput): Card[] {
  const size = input.size ?? SESSION_SIZE;
  const { reviews, today } = input;
  const pool = eligible(input.cards, input.completed);

  const due = pool
    .filter((c) => reviews[c.id] && isDue(reviews[c.id] as Review, today))
    .sort((a, b) => overdueDays(reviews[b.id] as Review, today) - overdueDays(reviews[a.id] as Review, today) || hashString(a.id) - hashString(b.id));
  const fresh = pool.filter((c) => !reviews[c.id]);
  const demoFresh = shuffled(fresh.filter((c) => c.source === "demo"), hashString(today + "d"));
  const csFresh = shuffled(fresh.filter((c) => c.source === "cs"), hashString(today + "c"));
  const mixed: Card[] = [];
  for (let i = 0; demoFresh.length > 0 || csFresh.length > 0; i++) {
    const wantCs = i % 3 === 2;
    const next = wantCs ? (csFresh.shift() ?? demoFresh.shift()) : (demoFresh.shift() ?? csFresh.shift());
    if (next) mixed.push(next);
  }

  const order = [...due.slice(0, Math.ceil(size * 0.6)), ...mixed, ...due.slice(Math.ceil(size * 0.6))];
  const out: Card[] = [];
  const seen = new Set<string>();
  for (const c of order) {
    const key = topicKey(c);
    if (seen.has(key)) continue;
    seen.add(key);
    out.push(c);
    if (out.length === size) break;
  }
  return out;
}
