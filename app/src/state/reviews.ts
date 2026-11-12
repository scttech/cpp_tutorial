import { answer } from "../warmup/schedule";
import { recordAnswer } from "../warmup/weak";
import type { Progress } from "./model";
import { bumpEvent } from "./recorder";
import { localDate, touchStreak } from "./streak";

/**
 * Fold one answered warm-up card into the progress: the card's review box and due date, the "warmup-answered" and
 * "warmup-correct" counters (the Warm-Up badges read the second), and the streak (a warm-up counts as a day of practice).
 * Pure and deterministic given `now`.
 */
export function applyReview(state: Progress, cardId: string, correct: boolean, now: Date, unit?: string): Progress {
  const today = localDate(now);
  const iso = now.toISOString();
  let next: Progress = { ...state, reviews: { ...state.reviews, [cardId]: answer(state.reviews[cardId], correct, today) } };
  if (unit) next = { ...next, weak: recordAnswer(state.weak, unit, correct, today) };
  next = bumpEvent(next, "warmup-answered", iso);
  if (correct) next = bumpEvent(next, "warmup-correct", iso);
  return { ...next, streak: touchStreak(next.streak, today) };
}
