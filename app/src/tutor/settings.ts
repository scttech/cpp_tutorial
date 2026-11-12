import { useSyncExternalStore } from "react";

// Tutor settings live in localStorage ONLY: never in the progress data, never exported, never sent anywhere
// except api.anthropic.com (spec 8.1).

export interface TutorSettings {
  apiKey: string;
  model: string;
  maxTokens: number;
}

export const MODELS = [
  { id: "claude-sonnet-5-5", label: "Claude Sonnet 5.5 (balanced, recommended)" },
  { id: "claude-haiku-5-5", label: "Claude Haiku 5.5 (fastest, cheapest)" },
  { id: "claude-opus-5-5", label: "Claude Opus 5.5 (most thorough)" },
  { id: "claude-fable-5-2", label: "Claude Fable 5.2" },
];

export const DEFAULTS: TutorSettings = { apiKey: "", model: "claude-sonnet-5-5", maxTokens: 1024 };
export const MAX_TOKENS_LIMIT = 4096;

const KEY = "zth.tutor.v1";
const LEAK_KEY = "zth.tutor.leaks.v1";

export function sanitize(raw: unknown): TutorSettings {
  const out = { ...DEFAULTS };
  if (typeof raw !== "object" || raw === null) return out;
  const r = raw as Record<string, unknown>;
  if (typeof r.apiKey === "string") out.apiKey = r.apiKey.trim();
  if (typeof r.model === "string" && /^[A-Za-z0-9._:-]{1,80}$/.test(r.model)) out.model = r.model;
  if (typeof r.maxTokens === "number" && Number.isFinite(r.maxTokens)) out.maxTokens = Math.min(MAX_TOKENS_LIMIT, Math.max(64, Math.floor(r.maxTokens)));
  return out;
}

function load(): TutorSettings {
  try {
    const raw = localStorage.getItem(KEY);
    return raw ? sanitize(JSON.parse(raw)) : { ...DEFAULTS };
  } catch {
    return { ...DEFAULTS };
  }
}

let current: TutorSettings = load();
const listeners = new Set<() => void>();
const subscribe = (l: () => void) => {
  listeners.add(l);
  return () => listeners.delete(l);
};

export const getTutorSettings = () => current;
export const useTutorSettings = (): TutorSettings => useSyncExternalStore(subscribe, () => current);

export function saveTutorSettings(patch: Partial<TutorSettings>) {
  current = sanitize({ ...current, ...patch });
  try {
    if (current.apiKey === "" && current.model === DEFAULTS.model && current.maxTokens === DEFAULTS.maxTokens) localStorage.removeItem(KEY);
    else localStorage.setItem(KEY, JSON.stringify(current));
  } catch {
    /* storage unavailable: the setting lasts for this session only */
  }
  listeners.forEach((l) => l());
}

export const clearApiKey = () => saveTutorSettings({ apiKey: "" });

/** A key that looks like an Anthropic key. Only a hint for the UI: the API is the judge. */
export const looksLikeKey = (k: string) => /^sk-ant-[A-Za-z0-9_-]{20,}$/.test(k);

// ---- local leak log (spec 8.5, step 3) ---------------------------------------------------------------
export interface LeakEntry {
  at: string;
  lesson: string;
  mode: string;
  reason: string;
  detail: string;
}

export function readLeaks(): LeakEntry[] {
  try {
    const v = JSON.parse(localStorage.getItem(LEAK_KEY) ?? "[]") as unknown;
    return Array.isArray(v) ? (v as LeakEntry[]) : [];
  } catch {
    return [];
  }
}

export function logLeak(e: LeakEntry) {
  try {
    localStorage.setItem(LEAK_KEY, JSON.stringify([...readLeaks(), e].slice(-50)));
  } catch {
    /* ignore */
  }
}

export function clearLeaks() {
  try {
    localStorage.removeItem(LEAK_KEY);
  } catch {
    /* ignore */
  }
}
