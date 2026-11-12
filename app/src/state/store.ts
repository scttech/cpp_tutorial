import { useSyncExternalStore } from "react";
import { badgeDefs, catalog } from "../badges/catalog";
import { awardBadges, newlyEarned } from "../badges/engine";
import type { BadgeDef } from "../badges/engine";
import { idbDelete, idbGet, idbSet } from "./idb";
import { emptyLesson, emptyProgress, normalize } from "./model";
import type { LessonProgress, Progress } from "./model";
import { cardById } from "../warmup/bank";
import { unitOf } from "../warmup/weak";
import { applyResolve } from "./resolves";
import { makePlan } from "./plan";
import { localDate } from "./streak";
import type { ResolveOutcome } from "./resolves";
import { applyReview } from "./reviews";
import { applyRun } from "./recorder";
import type { RunInput } from "./recorder";
import { pushToast } from "./toasts";

// ---- state ---------------------------------------------------------------------------------------
// Persistence: IndexedDB (key "progress"). If IndexedDB is unavailable (private windows, blocked site
// data) we fall back to localStorage, and if that fails too the app still works for the session.

export type StorageKind = "indexeddb" | "localstorage" | "memory";

const IDB_KEY = "progress";
const LS_KEY = "zth.progress.v2"; // fallback copy
const LEGACY_LS_KEY = "zth.progress.v1"; // first release stored progress here

let state: Progress = emptyProgress();
let storageKind: StorageKind = "memory";
let sessionCompileErrors = 0;
const listeners = new Set<() => void>();

const emit = () => listeners.forEach((l) => l());
const subscribe = (l: () => void) => {
  listeners.add(l);
  return () => listeners.delete(l);
};

// ---- persistence ---------------------------------------------------------------------------------
let timer: ReturnType<typeof setTimeout> | undefined;
let dirty = false;

async function persistNow(): Promise<void> {
  dirty = false;
  const snapshot = state;
  if (storageKind === "indexeddb") {
    try {
      await idbSet(IDB_KEY, snapshot);
      return;
    } catch {
      storageKind = "localstorage"; // IndexedDB started failing: degrade rather than lose data
      emit();
    }
  }
  if (storageKind === "localstorage") {
    try {
      localStorage.setItem(LS_KEY, JSON.stringify(snapshot));
    } catch {
      storageKind = "memory";
      emit();
    }
  }
}

function schedulePersist() {
  dirty = true;
  clearTimeout(timer);
  timer = setTimeout(() => void persistNow(), 250); // coalesce keystrokes
}

/** Write any pending change immediately (called when the tab is hidden or closing). */
export function flushPersist(): Promise<void> {
  clearTimeout(timer);
  return dirty ? persistNow() : Promise.resolve();
}

function commit(next: Progress) {
  state = next;
  schedulePersist();
  emit();
}

/** Load saved progress, migrating the first-release localStorage format. Call once before rendering. */
export async function initStore(): Promise<void> {
  let loaded: unknown;
  try {
    loaded = await idbGet(IDB_KEY);
    storageKind = "indexeddb";
  } catch {
    storageKind = "localstorage";
  }
  if (loaded === undefined) {
    // nothing in IndexedDB yet: look for the fallback copy, then for the first-release data
    for (const key of [LS_KEY, LEGACY_LS_KEY]) {
      try {
        const raw = localStorage.getItem(key);
        if (raw) {
          loaded = JSON.parse(raw);
          break;
        }
      } catch {
        /* unreadable: ignore */
      }
    }
    if (loaded !== undefined && storageKind === "indexeddb") {
      try {
        await idbSet(IDB_KEY, normalize(loaded));
        localStorage.removeItem(LEGACY_LS_KEY);
        localStorage.removeItem(LS_KEY);
      } catch {
        storageKind = "localstorage";
      }
    }
  }
  state = normalize(loaded);
  // Re-check badges silently: covers progress made before badges existed, or imported files.
  const earned = newlyEarned(badgeDefs, state, catalog);
  if (earned.length) {
    state = awardBadges(state, earned, new Date().toISOString());
    schedulePersist();
  }
  if (typeof document !== "undefined") {
    document.addEventListener("visibilitychange", () => {
      if (document.visibilityState === "hidden") void flushPersist();
    });
    window.addEventListener("pagehide", () => void flushPersist());
  }
}

// ---- reading -------------------------------------------------------------------------------------
const EMPTY_LESSON = emptyLesson();
export const getState = () => state;
export const useProgress = (id: string): LessonProgress => useSyncExternalStore(subscribe, () => state.lessons[id] ?? EMPTY_LESSON);
export const useAllProgress = (): Progress => useSyncExternalStore(subscribe, () => state);
export const useStorageKind = (): StorageKind => useSyncExternalStore(subscribe, () => storageKind);

// ---- writing -------------------------------------------------------------------------------------
/** Lesson-level update (code edits, hints, ...). */
export function update(id: string, fn: (p: LessonProgress) => LessonProgress) {
  const cur = state.lessons[id] ?? emptyLesson();
  commit({ ...state, lessons: { ...state.lessons, [id]: fn(cur) } });
}

export const revealHint = (id: string) => update(id, (p) => ({ ...p, hintsUsed: p.hintsUsed + 1 }));
/** One tutor request was answered: counts towards aiHelpUsed (and, for explain/discuss modes, towards Good Question). */
export const recordAiHelp = (id: string, explains: boolean) => update(id, (p) => ({ ...p, aiHelpUsed: p.aiHelpUsed + 1, explainUsed: p.explainUsed || explains }));
export const revealSolution = (id: string) => update(id, (p) => ({ ...p, solutionRevealed: true }));
export const setWeakClear = (weakClear: number) => commit({ ...state, settings: { ...state.settings, weakClear } });
export const setPace = (pace: "full" | "fast") => commit({ ...state, settings: { ...state.settings, pace } });

/** Record a finished run, award any newly earned badges, and announce them. */
export function recordRun(input: Omit<RunInput, "sessionCompileErrors">, now = new Date()): BadgeDef[] {
  const outcome = applyRun(state, { ...input, sessionCompileErrors }, now);
  sessionCompileErrors = outcome.sessionCompileErrors;
  const earned = newlyEarned(badgeDefs, outcome.state, catalog);
  commit(awardBadges(outcome.state, earned, now.toISOString()));
  const defs = earned.map((e) => badgeDefs.find((b) => b.id === e.id)).filter((b): b is BadgeDef => b !== undefined);
  for (const b of defs) pushToast({ kind: "badge", title: `Badge earned: ${b.name}${b.tier ? ` (${b.tier})` : ""}`, body: b.description, icon: b.icon, badgeId: b.id });
  return defs;
}

/** Make sure today has a plan; the first call of a day chooses it. `nextLessonId` is the lesson that would come next. */
export function ensureDailyPlan(nextLessonId: string | null, now = new Date()): void {
  const today = localDate(now);
  if (state.plan?.date === today) return;
  commit({ ...state, plan: makePlan(state, today, nextLessonId) });
}

/** Record a finished re-solve of a completed lesson (it never touches the saved solution). */
export function recordResolve(lessonId: string, outcome: ResolveOutcome, now = new Date()): void {
  const after = applyResolve(state, lessonId, outcome, now);
  const earned = newlyEarned(badgeDefs, after, catalog);
  commit(awardBadges(after, earned, now.toISOString()));
  for (const e of earned) {
    const b = badgeDefs.find((d) => d.id === e.id);
    if (b) pushToast({ kind: "badge", title: `Badge earned: ${b.name}${b.tier ? ` (${b.tier})` : ""}`, body: b.description, icon: b.icon, badgeId: b.id });
  }
}

/** Record the answer to a warm-up card, award any newly earned badge, and announce it. */
export function recordReview(cardId: string, correct: boolean, now = new Date()): BadgeDef[] {
  const card = cardById.get(cardId);
  const after = applyReview(state, cardId, correct, now, card ? unitOf(card) : undefined);
  const earned = newlyEarned(badgeDefs, after, catalog);
  commit(awardBadges(after, earned, now.toISOString()));
  const defs = earned.map((e) => badgeDefs.find((b) => b.id === e.id)).filter((b): b is BadgeDef => b !== undefined);
  for (const b of defs) pushToast({ kind: "badge", title: `Badge earned: ${b.name}${b.tier ? ` (${b.tier})` : ""}`, body: b.description, icon: b.icon, badgeId: b.id });
  return defs;
}

// ---- export / import / reset ----------------------------------------------------------------------
const EXPORT_APP = "cpp-zero-to-hero";

/** Progress as a JSON document. Contains no secrets (there is no API key in the progress data). */
export function exportProgress(): string {
  return JSON.stringify({ app: EXPORT_APP, exportedAt: new Date().toISOString(), progress: state }, null, 2);
}

export interface ImportSummary {
  lessons: number;
  complete: number;
  badges: number;
}

export function summarize(p: Progress): ImportSummary {
  return {
    lessons: Object.keys(p.lessons).length,
    complete: Object.values(p.lessons).filter((l) => l.status === "complete").length,
    badges: Object.keys(p.badges).length,
  };
}

/** Parse and validate an exported file WITHOUT applying it, so the UI can ask for confirmation. */
export function parseImport(text: string): { ok: true; progress: Progress; summary: ImportSummary } | { ok: false; error: string } {
  let data: unknown;
  try {
    data = JSON.parse(text);
  } catch {
    return { ok: false, error: "That file is not valid JSON." };
  }
  const doc = data as { app?: unknown; progress?: { version?: unknown } };
  if (typeof data !== "object" || data === null || doc.app !== EXPORT_APP || typeof doc.progress !== "object" || doc.progress === null) {
    return { ok: false, error: "That does not look like a progress export from this app." };
  }
  if (doc.progress.version !== 1) return { ok: false, error: `Unsupported progress version: ${String(doc.progress.version)}.` };
  const progress = normalize(doc.progress);
  return { ok: true, progress, summary: summarize(progress) };
}

/** Replace all progress with an already validated import. */
export function applyImport(progress: Progress) {
  sessionCompileErrors = 0;
  commit(progress);
  void flushPersist();
}

export async function resetAll() {
  sessionCompileErrors = 0;
  clearTimeout(timer);
  state = emptyProgress();
  dirty = false;
  emit();
  try {
    await idbDelete(IDB_KEY);
  } catch {
    /* fine: fallback below */
  }
  try {
    localStorage.removeItem(LS_KEY);
    localStorage.removeItem(LEGACY_LS_KEY);
  } catch {
    /* ignore */
  }
}
