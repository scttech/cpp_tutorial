import type { RunResult, Tier } from "../runner/types";
import { bumpActivity } from "./activity";
import { emptyLesson } from "./model";
import type { LessonProgress, Progress } from "./model";
import { localDate, touchStreak } from "./streak";

/** A test whose name carries this tag fails on a classic off-by-one bug (the hidden "Off By One" badge). */
export const OFF_BY_ONE_TAG = "(off-by-one)";

/** Code lines of the learner's own files: blank lines and whole-line // comments do not count (for the Refactorer badge). */
export function countCodeLines(files: Record<string, string>, editable: string[]): number {
  let n = 0;
  for (const name of editable) {
    for (const line of (files[name] ?? "").split("\n")) {
      const t = line.trim();
      if (t !== "" && !t.startsWith("//")) n += 1;
    }
  }
  return n;
}

export interface RunInput {
  lessonId: string;
  mode: "run" | "test";
  declaredTiers: Tier[];
  result: Pick<RunResult, "compiled" | "tests" | "signal" | "timedOut" | "diagnostics">;
  /** Compiler errors seen since the last clean compile this session (for the "It Compiles!" badge). */
  sessionCompileErrors: number;
  /** Code lines of the learner's editable files in this run (countCodeLines). Optional: without it nothing about refactoring is tracked. */
  userLines?: number;
}

export interface RunOutcome {
  state: Progress;
  sessionCompileErrors: number;
  becameComplete: boolean;
}

export function bumpEvent(state: Progress, name: string, nowIso: string, by = 1): Progress {
  const cur = state.events[name]?.count ?? 0;
  return { ...state, events: { ...state.events, [name]: { count: cur + by, lastAt: nowIso } } };
}

/**
 * Fold one finished run into the progress state: attempts, earned tiers, completion, failed-run
 * counting, streak and fun events. Pure and deterministic given `now`.
 */
export function applyRun(state: Progress, input: RunInput, now: Date): RunOutcome {
  const nowIso = now.toISOString();
  const { result, mode, declaredTiers } = input;
  const before: LessonProgress = state.lessons[input.lessonId] ?? emptyLesson();
  const p: LessonProgress = { ...before, bestTiers: { ...before.bestTiers }, attempts: before.attempts + 1 };
  if (mode === "test") p.testRuns += 1;

  // Tiers: a tier is earned only when ALL of its tests pass in one run. A run that the time limit killed reports only the tests that
  // finished before it: they may all have passed while others never ran, so such a run earns nothing.
  const allPass = result.compiled && !result.timedOut && result.tests.length > 0 && result.tests.every((t) => t.status === "pass");
  if (mode === "test" && result.compiled && !result.timedOut) {
    for (const tier of declaredTiers) {
      const rows = result.tests.filter((t) => (t.tier ?? "works") === tier);
      if (rows.length > 0 && rows.every((t) => t.status === "pass")) p.bestTiers[tier] = true;
    }
  }

  // Wall of Warnings: warnings that disappeared between two successful builds of this lesson count as fixed
  const warnings = result.diagnostics.filter((d) => d.severity === "warning").length;
  let fixedWarnings = 0;
  if (result.compiled) {
    if (before.lastWarnings !== undefined && warnings < before.lastWarnings) fixedWarnings = before.lastWarnings - warnings;
    p.lastWarnings = warnings;
  }

  const wasComplete = before.status === "complete";
  const complete = wasComplete || (declaredTiers.length > 0 && declaredTiers.every((t) => p.bestTiers[t]));
  if (mode === "test" && !wasComplete && !allPass) p.failedRuns += 1;
  p.status = complete ? "complete" : "in_progress";
  const becameComplete = complete && !wasComplete;
  if (becameComplete) {
    p.firstCompleteAt = nowIso;
    p.runsToFirstPass = p.testRuns;
  }

  // Refactorer: a passing run of an already-completed lesson with fewer code lines than any passing version before
  let refactored = false;
  if (input.userLines !== undefined && mode === "test" && allPass) {
    if (p.bestLines === undefined) p.bestLines = input.userLines; // the first passing version is the baseline
    else if (input.userLines < p.bestLines) {
      p.bestLines = input.userLines;
      refactored = wasComplete;
    }
  }

  let next: Progress = { ...state, lessons: { ...state.lessons, [input.lessonId]: p } };
  if (refactored) next = bumpEvent(next, "refactor", nowIso);
  if (fixedWarnings > 0) next = bumpEvent(next, "warnings-fixed", nowIso, fixedWarnings);
  if (mode === "test" && result.compiled && result.tests.some((t) => t.status !== "pass" && t.name.includes(OFF_BY_ONE_TAG))) next = bumpEvent(next, "off-by-one", nowIso);
  next = bumpActivity(next, localDate(now), "runs");
  if (becameComplete) next = bumpActivity(next, localDate(now), "lessons");
  next = { ...next, streak: touchStreak(next.streak, localDate(now)) };

  // Fun events (hidden badges)
  const crashed = result.signal === "SIGSEGV" || result.tests.some((t) => t.status === "error" && /segmentation fault/i.test(t.message ?? ""));
  if (crashed) next = bumpEvent(next, "segfault", nowIso);
  if (result.timedOut || result.tests.some((t) => t.status === "timeout")) next = bumpEvent(next, "timeout", nowIso);

  let sessionCompileErrors = input.sessionCompileErrors;
  if (!result.compiled) {
    sessionCompileErrors += result.diagnostics.filter((d) => d.severity === "error").length;
  } else {
    if (sessionCompileErrors >= 10) next = bumpEvent(next, "compiled-after-many-errors", nowIso);
    sessionCompileErrors = 0;
  }

  // Good Question: asked the tutor to explain, then passed without revealing the solution
  if (becameComplete && before.explainUsed && !before.solutionRevealed) next = bumpEvent(next, "good-question", nowIso);

  if (becameComplete) {
    const hour = now.getHours();
    if (hour < 4) next = bumpEvent(next, "completed-at-night", nowIso);
    else if (hour < 6) next = bumpEvent(next, "completed-early-morning", nowIso);
  }

  return { state: next, sessionCompileErrors, becameComplete };
}
