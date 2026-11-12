import { bumpActivity } from "./activity";
import { emptyLesson } from "./model";
import type { Progress } from "./model";
import { localDate } from "./streak";

/** The most time one report may add (a laptop that slept must not count as study time). */
export const MAX_SLICE_MS = 5 * 60_000;

/** Add `ms` of working time to a lesson and to the day. Pure given `now`. */
export function applyTime(state: Progress, lessonId: string, ms: number, now: Date): Progress {
  const add = Math.min(MAX_SLICE_MS, Math.max(0, Math.round(ms)));
  if (add === 0) return state;
  const cur = state.lessons[lessonId] ?? emptyLesson();
  const next: Progress = { ...state, lessons: { ...state.lessons, [lessonId]: { ...cur, timeMs: cur.timeMs + add } } };
  return bumpActivity(next, localDate(now), "ms", add);
}
