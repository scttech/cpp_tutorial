import { useSyncExternalStore } from "react";
import type { Tier } from "../runner/types";

// Minimal localStorage-backed store so you can test the loop now. M3 replaces this with the
// IndexedDB store + export/import described in the spec (section 2.4); the shape is the same.
export interface LessonProgress {
  status: "available" | "in_progress" | "complete";
  bestTiers: Record<Tier, boolean>;
  attempts: number;
  testRuns: number;
  hintsUsed: number;
  solutionRevealed: boolean;
  lastCode: Record<string, string>;
  firstCompleteAt?: string;
}

interface State {
  version: 1;
  lessons: Record<string, LessonProgress>;
}

const KEY = "zth.progress.v1";
const empty = (): LessonProgress => ({
  status: "available",
  bestTiers: { works: false, robust: false, fast: false, clean: false },
  attempts: 0,
  testRuns: 0,
  hintsUsed: 0,
  solutionRevealed: false,
  lastCode: {},
});

function load(): State {
  try {
    const raw = localStorage.getItem(KEY);
    if (raw) {
      const s = JSON.parse(raw) as State;
      if (s.version === 1) return s;
    }
  } catch {
    /* storage unavailable or corrupt: start fresh */
  }
  return { version: 1, lessons: {} };
}

let state = load();
const listeners = new Set<() => void>();

function commit(next: State) {
  state = next;
  try {
    localStorage.setItem(KEY, JSON.stringify(state));
  } catch {
    /* ignore quota / private mode */
  }
  listeners.forEach((l) => l());
}

export function update(id: string, fn: (p: LessonProgress) => LessonProgress) {
  const cur = state.lessons[id] ?? empty();
  commit({ ...state, lessons: { ...state.lessons, [id]: fn(cur) } });
}

export function resetAll() {
  commit({ version: 1, lessons: {} });
}

const subscribe = (l: () => void) => {
  listeners.add(l);
  return () => listeners.delete(l);
};

export const useProgress = (id: string): LessonProgress =>
  useSyncExternalStore(subscribe, () => state.lessons[id] ?? EMPTY);
const EMPTY = empty();

export const useAllProgress = () => useSyncExternalStore(subscribe, () => state);
