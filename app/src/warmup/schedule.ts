import { daysBetween } from "../state/streak";

// Spaced review with Leitner boxes. A card sits in box 1 to 5; the box decides how many days pass before it is asked again.
// A right answer moves it up a box, a wrong one sends it back to box 1 (asked again tomorrow).

/** Days until the next review for a card now in box 1, 2, 3, 4 or 5. */
export const INTERVALS = [1, 3, 7, 14, 30] as const;
export const MAX_BOX = INTERVALS.length;

export interface Review {
  box: number;
  /** Local date (YYYY-MM-DD) from which the card is due again. */
  due: string;
  seen: number;
  correct: number;
}

/** `date` plus n whole days, as YYYY-MM-DD. Computed in UTC so daylight-saving shifts do not matter. */
export function addDays(date: string, n: number): string {
  const [y, m, d] = date.split("-").map(Number) as [number, number, number];
  const t = new Date(Date.UTC(y, m - 1, d + n));
  const p = (x: number) => String(x).padStart(2, "0");
  return `${t.getUTCFullYear()}-${p(t.getUTCMonth() + 1)}-${p(t.getUTCDate())}`;
}

/** The review state after one answer on `today`. A card seen for the first time that is answered right starts in box 2. */
export function answer(prev: Review | undefined, correct: boolean, today: string): Review {
  const seen = (prev?.seen ?? 0) + 1;
  const total = (prev?.correct ?? 0) + (correct ? 1 : 0);
  const box = correct ? Math.min(MAX_BOX, (prev?.box ?? 1) + 1) : 1;
  return { box, due: addDays(today, INTERVALS[box - 1] as number), seen, correct: total };
}

export const isDue = (r: Review, today: string) => daysBetween(r.due, today) >= 0;

/** How many days late a due card is (0 when due today, negative when not yet due). */
export const overdueDays = (r: Review, today: string) => daysBetween(r.due, today);
