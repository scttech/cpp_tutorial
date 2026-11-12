// Display and editor preferences. Kept apart from progress: they are per-browser, never exported with progress,
// and must apply before the first paint so the page never flashes the wrong theme.
import { useSyncExternalStore } from "react";

export type ThemePref = "dark" | "light" | "contrast" | "system";
export type ResolvedTheme = "dark" | "light" | "contrast";
export type MotionPref = "system" | "reduce";
export type KeyMode = "default" | "vim" | "emacs";
/** Lesson page: "auto" puts the results beside the editor on a wide screen, "wide" always does, "normal" never does. */
export type LayoutPref = "auto" | "wide" | "normal";

export interface Prefs {
  theme: ThemePref;
  /** Page text size, percent of the browser default. */
  textScale: number;
  motion: MotionPref;
  /** Editor font size in px. */
  editorFont: number;
  wordWrap: boolean;
  keys: KeyMode;
  layout: LayoutPref;
}

export const DEFAULT_PREFS: Prefs = { theme: "dark", textScale: 100, motion: "system", editorFont: 14, wordWrap: false, keys: "default", layout: "auto" };
export const TEXT_SCALE = { min: 90, max: 150, step: 10 } as const;
export const EDITOR_FONT = { min: 12, max: 24 } as const;
export const PREFS_KEY = "zth.prefs.v1";

const THEMES: ThemePref[] = ["dark", "light", "contrast", "system"];
const MOTIONS: MotionPref[] = ["system", "reduce"];
const KEYS: KeyMode[] = ["default", "vim", "emacs"];
const LAYOUTS: LayoutPref[] = ["auto", "wide", "normal"];
/** The screen width (px) from which "auto" uses the wide lesson layout. */
export const WIDE_FROM = 1800;

const clampInt = (v: unknown, min: number, max: number, fallback: number): number => {
  if (typeof v !== "number" || !Number.isFinite(v)) return fallback;
  return Math.min(max, Math.max(min, Math.round(v)));
};

/** Turn anything read from storage into a valid Prefs; unknown or malformed fields fall back to the defaults. */
export function sanitizePrefs(raw: unknown): Prefs {
  const o = (typeof raw === "object" && raw !== null ? raw : {}) as Record<string, unknown>;
  return {
    theme: THEMES.includes(o.theme as ThemePref) ? (o.theme as ThemePref) : DEFAULT_PREFS.theme,
    textScale: clampInt(o.textScale, TEXT_SCALE.min, TEXT_SCALE.max, DEFAULT_PREFS.textScale),
    motion: MOTIONS.includes(o.motion as MotionPref) ? (o.motion as MotionPref) : DEFAULT_PREFS.motion,
    editorFont: clampInt(o.editorFont, EDITOR_FONT.min, EDITOR_FONT.max, DEFAULT_PREFS.editorFont),
    wordWrap: o.wordWrap === true,
    keys: KEYS.includes(o.keys as KeyMode) ? (o.keys as KeyMode) : DEFAULT_PREFS.keys,
    layout: LAYOUTS.includes(o.layout as LayoutPref) ? (o.layout as LayoutPref) : DEFAULT_PREFS.layout,
  };
}

export const resolveLayout = (pref: LayoutPref, wideScreen: boolean): "wide" | "normal" => (pref === "auto" ? (wideScreen ? "wide" : "normal") : pref);

/** What "system" means: the OS asks for more contrast, a light scheme, or neither (dark, as the site always was). */
export function resolveTheme(pref: ThemePref, media: { contrast: boolean; light: boolean }): ResolvedTheme {
  if (pref !== "system") return pref;
  if (media.contrast) return "contrast";
  return media.light ? "light" : "dark";
}

const matches = (q: string): boolean => typeof window !== "undefined" && typeof window.matchMedia === "function" && window.matchMedia(q).matches;

function load(): Prefs {
  try {
    const text = localStorage.getItem(PREFS_KEY);
    return text ? sanitizePrefs(JSON.parse(text)) : { ...DEFAULT_PREFS };
  } catch {
    return { ...DEFAULT_PREFS };
  }
}

let current: Prefs = load();
const listeners = new Set<() => void>();

export function applyPrefs(p: Prefs, root: HTMLElement = document.documentElement): void {
  root.dataset.theme = resolveTheme(p.theme, { contrast: matches("(prefers-contrast: more)"), light: matches("(prefers-color-scheme: light)") });
  root.dataset.layout = resolveLayout(p.layout, matches(`(min-width: ${WIDE_FROM}px)`));
  root.style.fontSize = `${p.textScale}%`;
  if (p.motion === "reduce") root.dataset.motion = "reduce";
  else delete root.dataset.motion;
}

export const getPrefs = (): Prefs => current;

export function savePrefs(patch: Partial<Prefs>): void {
  current = sanitizePrefs({ ...current, ...patch });
  try {
    localStorage.setItem(PREFS_KEY, JSON.stringify(current));
  } catch {
    // Storage is unavailable (a private window): the choice still holds for this visit.
  }
  applyPrefs(current);
  listeners.forEach((l) => l());
}

export function resetPrefs(): void {
  savePrefs({ ...DEFAULT_PREFS });
}

/** Apply the saved preferences and follow the operating system while the theme is "system". Call once, before the first render. */
export function initPrefs(): void {
  applyPrefs(current);
  if (typeof window === "undefined" || typeof window.matchMedia !== "function") return;
  for (const q of ["(prefers-contrast: more)", "(prefers-color-scheme: light)", `(min-width: ${WIDE_FROM}px)`]) {
    window.matchMedia(q).addEventListener("change", () => {
      if (current.theme === "system" || current.layout === "auto") {
        applyPrefs(current);
        listeners.forEach((l) => l());
      }
    });
  }
}

export const subscribePrefs = (l: () => void): (() => void) => {
  listeners.add(l);
  return () => listeners.delete(l);
};

export const usePrefs = (): Prefs => useSyncExternalStore(subscribePrefs, getPrefs, getPrefs);

/** The theme actually in force (resolves "system"). */
export const activeTheme = (): ResolvedTheme => (document.documentElement.dataset.theme as ResolvedTheme) || "dark";
