import type { LessonType } from "../content/types";
import type { Tier } from "../runner/types";
import { emptyLesson } from "../state/model";
import type { LessonProgress, Progress } from "../state/model";
import { localDate } from "../state/streak";

// ---- definitions (loaded from content/badges/*.yaml) -------------------------------------------

export interface Where {
  status?: LessonProgress["status"];
  type?: LessonType[];
  module?: string;
  hintsUsed?: number;
  aiHelpUsed?: number;
  solutionRevealed?: boolean;
  allTiers?: boolean;
}

export type Rule =
  | { type: "lesson-complete"; lesson: string }
  | { type: "lesson-count"; where: Where; atLeast: number }
  | { type: "module-complete"; module: string }
  | { type: "tier-complete"; tier: Tier; atLeast: number }
  | { type: "first-try"; atLeast: number }
  | { type: "persistence"; failedRunsAtLeast: number }
  | { type: "streak-days"; atLeast: number }
  | { type: "custom-event"; event: string; atLeast: number }
  | { type: "same-day-count"; where: Where; atLeast: number }
  | { type: "all"; rules: Rule[] };

export interface BadgeDef {
  id: string;
  name: string;
  category: "progress" | "mastery" | "habits" | "consistency" | "fun";
  tier?: "bronze" | "silver" | "gold";
  icon: string;
  description: string;
  hidden?: boolean;
  rule: Rule;
}

/** The slice of course content the engine needs to know about. */
export interface CatalogLesson {
  id: string;
  module: string;
  type: LessonType;
  tiers: Record<Tier, boolean>;
}
export interface Catalog {
  lessons: CatalogLesson[];
  /** Modules whose content is still being written (finishing the lessons that exist is not enough). */
  inProgressModules?: string[];
}

// ---- evaluation ---------------------------------------------------------------------------------

export interface Evaluation {
  met: boolean;
  /** Lessons that counted towards the rule (shown as the badge's "evidence"). */
  lessons: string[];
  /** For count-like rules: how far along the learner is. */
  current?: number;
  target?: number;
}

const prog = (state: Progress, id: string): LessonProgress => state.lessons[id] ?? emptyLesson();

function matches(l: CatalogLesson, p: LessonProgress, w: Where): boolean {
  if (w.status !== undefined && p.status !== w.status) return false;
  if (w.type !== undefined && !w.type.includes(l.type)) return false;
  if (w.module !== undefined && l.module !== w.module) return false;
  if (w.hintsUsed !== undefined && p.hintsUsed !== w.hintsUsed) return false;
  if (w.aiHelpUsed !== undefined && p.aiHelpUsed !== w.aiHelpUsed) return false;
  if (w.solutionRevealed !== undefined && p.solutionRevealed !== w.solutionRevealed) return false;
  if (w.allTiers === true && !(["works", "robust", "fast", "clean"] as Tier[]).every((t) => l.tiers[t] && p.bestTiers[t])) return false;
  return true;
}

/**
 * A module is complete when its test-out is passed, or when every other lesson in it is complete.
 * While a module is still being written (`inProgressModules`), only the test-out counts: finishing the
 * lessons that exist so far must not complete a module that has more to come.
 */
export function isModuleComplete(state: Progress, catalog: Catalog, moduleId: string): boolean {
  const inModule = catalog.lessons.filter((l) => l.module === moduleId);
  if (inModule.length === 0) return false;
  const done = (l: CatalogLesson) => prog(state, l.id).status === "complete";
  if (inModule.some((l) => l.type === "test-out" && done(l))) return true;
  if (catalog.inProgressModules?.includes(moduleId)) return false;
  const regular = inModule.filter((l) => l.type !== "test-out");
  return regular.length > 0 && regular.every(done);
}

const count = (ids: string[], atLeast: number): Evaluation => ({ met: ids.length >= atLeast, lessons: ids, current: ids.length, target: atLeast });

export function evaluate(rule: Rule, state: Progress, catalog: Catalog): Evaluation {
  switch (rule.type) {
    case "lesson-complete": {
      const ok = prog(state, rule.lesson).status === "complete";
      return { met: ok, lessons: ok ? [rule.lesson] : [] };
    }
    case "lesson-count": {
      const ids = catalog.lessons.filter((l) => matches(l, prog(state, l.id), rule.where)).map((l) => l.id);
      return count(ids, rule.atLeast);
    }
    case "module-complete": {
      const ok = isModuleComplete(state, catalog, rule.module);
      return { met: ok, lessons: ok ? catalog.lessons.filter((l) => l.module === rule.module).map((l) => l.id) : [] };
    }
    case "tier-complete": {
      const ids = catalog.lessons.filter((l) => l.tiers[rule.tier] && prog(state, l.id).bestTiers[rule.tier]).map((l) => l.id);
      return count(ids, rule.atLeast);
    }
    case "first-try": {
      const ids = catalog.lessons.filter((l) => {
        const p = prog(state, l.id);
        return p.status === "complete" && p.runsToFirstPass === 1;
      }).map((l) => l.id);
      return count(ids, rule.atLeast);
    }
    case "persistence": {
      const ids = catalog.lessons.filter((l) => {
        const p = prog(state, l.id);
        return p.status === "complete" && p.failedRuns >= rule.failedRunsAtLeast && !p.solutionRevealed;
      }).map((l) => l.id);
      return { met: ids.length >= 1, lessons: ids, current: Math.min(1, ids.length), target: 1 };
    }
    case "streak-days":
      return { met: state.streak.longest >= rule.atLeast, lessons: [], current: state.streak.longest, target: rule.atLeast };
    case "custom-event": {
      const n = state.events[rule.event]?.count ?? 0;
      return { met: n >= rule.atLeast, lessons: [], current: Math.min(n, rule.atLeast), target: rule.atLeast };
    }
    case "same-day-count": {
      const byDay = new Map<string, string[]>();
      for (const l of catalog.lessons) {
        const p = prog(state, l.id);
        if (p.status !== "complete" || !p.firstCompleteAt || !matches(l, p, rule.where)) continue;
        const day = localDate(new Date(p.firstCompleteAt));
        byDay.set(day, [...(byDay.get(day) ?? []), l.id]);
      }
      const best = [...byDay.values()].sort((a, b) => b.length - a.length)[0] ?? [];
      return count(best, rule.atLeast);
    }
    case "all": {
      const parts = rule.rules.map((r) => evaluate(r, state, catalog));
      return { met: parts.every((p) => p.met), lessons: [...new Set(parts.flatMap((p) => p.lessons))] };
    }
  }
}

/** Badge ids whose rules are newly met and not yet earned. */
export function newlyEarned(defs: BadgeDef[], state: Progress, catalog: Catalog): { id: string; lessons: string[] }[] {
  const out: { id: string; lessons: string[] }[] = [];
  for (const def of defs) {
    if (state.badges[def.id]) continue;
    const ev = evaluate(def.rule, state, catalog);
    if (ev.met) out.push({ id: def.id, lessons: ev.lessons });
  }
  return out;
}

/** Does the content this rule refers to exist yet? (Otherwise the badge is shown as "coming soon".) */
export function isAvailable(rule: Rule, catalog: Catalog): boolean {
  switch (rule.type) {
    case "lesson-complete":
      return catalog.lessons.some((l) => l.id === rule.lesson);
    case "module-complete":
      return catalog.lessons.some((l) => l.module === rule.module) && !catalog.inProgressModules?.includes(rule.module);
    case "lesson-count":
    case "same-day-count":
      if (rule.where.module !== undefined && !catalog.lessons.some((l) => l.module === rule.where.module)) return false;
      if (rule.where.type !== undefined && !catalog.lessons.some((l) => rule.where.type!.includes(l.type))) return false;
      return true;
    case "tier-complete":
      return catalog.lessons.some((l) => l.tiers[rule.tier]);
    case "all":
      return rule.rules.every((r) => isAvailable(r, catalog));
    default:
      return true;
  }
}

/** Marks earned badges in state with their evidence. Pure: returns a new state. */
export function awardBadges(state: Progress, earned: { id: string; lessons: string[] }[], nowIso: string): Progress {
  if (earned.length === 0) return state;
  const badges = { ...state.badges };
  for (const e of earned) badges[e.id] = { earnedAt: nowIso, evidence: e.lessons.length ? { lessons: e.lessons } : undefined };
  return { ...state, badges };
}

const RULE_TYPES = new Set(["lesson-complete", "lesson-count", "module-complete", "tier-complete", "first-try", "persistence", "streak-days", "custom-event", "same-day-count", "all"]);

/** Structural check of a rule from YAML; returns a list of problems (empty when fine). */
export function checkRule(rule: unknown, path = "rule"): string[] {
  if (typeof rule !== "object" || rule === null) return [`${path}: not an object`];
  const r = rule as Record<string, unknown>;
  if (typeof r.type !== "string" || !RULE_TYPES.has(r.type)) return [`${path}: unknown rule type "${String(r.type)}"`];
  const need = (k: string, t: string) => (typeof r[k] === t ? [] : [`${path}: "${r.type}" needs ${k} (${t})`]);
  switch (r.type) {
    case "lesson-complete":
      return need("lesson", "string");
    case "module-complete":
      return need("module", "string");
    case "lesson-count":
    case "same-day-count":
      return [...need("atLeast", "number"), ...(typeof r.where === "object" && r.where !== null ? [] : [`${path}: needs where`])];
    case "tier-complete":
      return [...need("tier", "string"), ...need("atLeast", "number")];
    case "first-try":
    case "streak-days":
      return need("atLeast", "number");
    case "persistence":
      return need("failedRunsAtLeast", "number");
    case "custom-event":
      return [...need("event", "string"), ...need("atLeast", "number")];
    case "all":
      return Array.isArray(r.rules) ? r.rules.flatMap((x, i) => checkRule(x, `${path}.rules[${i}]`)) : [`${path}: all needs rules[]`];
  }
  return [];
}
