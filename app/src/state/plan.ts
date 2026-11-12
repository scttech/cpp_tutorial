import { SESSION_SIZE } from "../warmup/session";
import { activityOn } from "./activity";
import type { Progress } from "./model";
import { dueResolves } from "./resolves";
import { localDate } from "./streak";

// Today's plan: a short, mixed day. Review first (a warm-up set, a lesson to re-solve), then one new lesson. The choice of lesson is made
// once per day and saved, so it stays put (and shows as done) even after the lesson is finished.

export interface DailyPlan {
  /** The local date this plan is for. */
  date: string;
  /** The new lesson chosen for the day, if any is left. */
  lessonId?: string;
  /** A lesson that was due for a re-solve when the day's plan was made, if any. */
  resolveId?: string;
}

export type PlanItemId = "warmup" | "resolve" | "lesson";

export interface PlanItem {
  id: PlanItemId;
  done: boolean;
  /** Short status such as "2 of 5 cards". */
  detail: string;
  lessonId?: string;
}

/** Warm-up cards that complete the warm-up step. */
export const PLAN_WARMUPS = SESSION_SIZE;

/** What to put in a new day's plan (pure: the caller says which lesson is next). */
export function makePlan(p: Progress, today: string, nextLessonId: string | null): DailyPlan {
  const due = dueResolves(p, today)[0];
  return { date: today, ...(nextLessonId ? { lessonId: nextLessonId } : {}), ...(due ? { resolveId: due.lessonId } : {}) };
}

/**
 * The steps of today's plan with their state. `warmupsAvailable` is whether any card is due or new; the warm-up step is left out
 * when there is nothing to ask and nothing was answered today.
 */
export function planItems(p: Progress, today: string, warmupsAvailable: boolean): PlanItem[] {
  const plan = p.plan;
  if (!plan || plan.date !== today) return [];
  const act = activityOn(p, today);
  const items: PlanItem[] = [];
  if (warmupsAvailable || act.warmups > 0) {
    const n = Math.min(act.warmups, PLAN_WARMUPS);
    items.push({ id: "warmup", done: act.warmups >= PLAN_WARMUPS, detail: `${n} of ${PLAN_WARMUPS} cards` });
  }
  if (plan.resolveId) {
    const last = p.resolves[plan.resolveId]?.lastAt;
    const done = !!last && localDate(new Date(last)) === today;
    items.push({ id: "resolve", done, detail: done ? "re-solved today" : "due for a re-solve", lessonId: plan.resolveId });
  }
  if (plan.lessonId) {
    const done = p.lessons[plan.lessonId]?.status === "complete";
    const started = p.lessons[plan.lessonId]?.status === "in_progress";
    items.push({ id: "lesson", done, detail: done ? "completed" : started ? "in progress" : "new", lessonId: plan.lessonId });
  }
  return items;
}

export const planComplete = (items: PlanItem[]): boolean => items.length > 0 && items.every((i) => i.done);
