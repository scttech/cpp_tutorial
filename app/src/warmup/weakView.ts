import { allLessons } from "../content/loader";
import type { Progress } from "../state/model";
import { localDate } from "../state/streak";
import { allCards } from "./bank";
import { pickPractice, weakSpots } from "./weak";
import type { WeakSpot } from "./weak";

const titles = new Map(allLessons.map((l) => [l.meta.id, l.meta.title]));

export const completedOf = (p: Progress): Set<string> => new Set(Object.entries(p.lessons).filter(([, l]) => l.status === "complete").map(([id]) => id));

/** The learner's current weak spots, worst first. */
export function weakSpotsFor(p: Progress, today = localDate(new Date())): WeakSpot[] {
  const friction = Object.fromEntries(Object.entries(p.lessons).filter(([, l]) => l.status === "complete"));
  return weakSpots({ cards: allCards, weak: p.weak, friction, completed: completedOf(p), titleOf: (id) => titles.get(id) ?? id, clearAfter: p.settings.weakClear, today });
}

/** A focused practice set: `which` is "all" (every weak spot) or one unit key. */
export function practiceSet(p: Progress, which: string, today = localDate(new Date())) {
  const spots = weakSpotsFor(p, today);
  const units = new Set(which === "all" ? spots.map((s) => s.unit) : spots.filter((s) => s.unit === which).map((s) => s.unit));
  return pickPractice({ cards: allCards, reviews: p.reviews, completed: completedOf(p), units, today });
}
