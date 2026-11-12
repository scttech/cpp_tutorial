import type { Lesson, LessonType, Module } from "./types";

// Course search: every word of the query must match the START of a word in the lesson's title, concepts, gotchas or module name
// (so "ptr" finds "unique_ptr" and "slicing" finds "slices"). Ranking: title first, then concepts, then gotcha titles, then the rest.

/** Split text into lower-case words. `_`, `-`, `:` and spaces separate; `+` and `#` stay (c++, c#). */
export function words(text: string): string[] {
  return text
    .toLowerCase()
    .split(/[^a-z0-9+#]+/)
    .filter((w) => w.length > 0);
}

/** A crude stem, enough to make "slicing", "slices" and "slice" agree. */
export function stem(w: string): string {
  if (w.length <= 4) return w;
  for (const suffix of ["ing", "ed", "es", "s"]) {
    if (w.endsWith(suffix) && w.length - suffix.length >= 3) return stem1(w.slice(0, -suffix.length));
  }
  return stem1(w);
}
const stem1 = (w: string) => (w.length > 3 && w.endsWith("e") ? w.slice(0, -1) : w);

export interface SearchEntry {
  id: string;
  moduleId: string;
  moduleTitle: string;
  title: string;
  type: LessonType;
  /** Position in the course (module order, then lesson order): the tie-break of the ranking. */
  rank: number;
  fields: { title: string[]; concepts: string[]; gotchas: string[]; rest: string[] };
  /** For "matched in ..." hints. */
  concepts: string[];
  gotchaTitles: string[];
}

export function indexLessons(modules: Module[]): SearchEntry[] {
  const out: SearchEntry[] = [];
  let rank = 0;
  for (const m of modules) {
    for (const l of m.lessons) out.push(entryOf(m, l, rank++));
  }
  return out;
}

function entryOf(m: Module, l: Lesson, rank: number): SearchEntry {
  const concepts = l.meta.concepts ?? [];
  const gotchaTitles = l.gotchas.map((g) => g.title);
  return {
    id: l.meta.id,
    moduleId: m.id,
    moduleTitle: m.title,
    title: l.meta.title,
    type: l.meta.type,
    rank,
    concepts,
    gotchaTitles,
    fields: {
      title: words(l.meta.title).map(stem),
      concepts: concepts.flatMap(words).map(stem),
      gotchas: gotchaTitles.flatMap(words).map(stem),
      rest: [...words(m.title), ...l.gotchas.flatMap((g) => words(g.text ?? ""))].map(stem),
    },
  };
}

export interface Hit {
  entry: SearchEntry;
  score: number;
  /** Where the best match was found, for a hint under the result. */
  via?: string;
}

const WEIGHTS = { title: 8, concepts: 4, gotchas: 2, rest: 1 } as const;

/** Lessons matching the query, best first. An empty query matches everything in course order. */
export function searchEntries(entries: SearchEntry[], query: string): Hit[] {
  const q = words(query).map(stem);
  if (q.length === 0) return entries.map((entry) => ({ entry, score: 0 }));
  const hits: Hit[] = [];
  for (const entry of entries) {
    let score = 0;
    let best = 0;
    let via: string | undefined;
    let ok = true;
    for (const t of q) {
      let found = 0;
      let where: keyof typeof WEIGHTS | undefined;
      for (const f of ["title", "concepts", "gotchas", "rest"] as const) {
        if (entry.fields[f].some((w) => w.startsWith(t))) {
          found = WEIGHTS[f];
          where = f;
          break;
        }
      }
      if (!where) {
        ok = false;
        break;
      }
      score += found;
      if (found > 0 && (via === undefined || (where !== "title" && found >= best))) {
        best = found;
        via = where === "title" ? undefined : hint(entry, t, where);
      }
    }
    if (ok) hits.push({ entry, score, via });
  }
  return hits.sort((a, b) => b.score - a.score || a.entry.rank - b.entry.rank);
}

function hint(entry: SearchEntry, token: string, where: "concepts" | "gotchas" | "rest"): string | undefined {
  if (where === "concepts") {
    const c = entry.concepts.find((x) => words(x).some((w) => stem(w).startsWith(token)));
    return c ? `concept: ${c}` : undefined;
  }
  if (where === "gotchas") {
    const g = entry.gotchaTitles.find((x) => words(x).some((w) => stem(w).startsWith(token)));
    return g ? `gotcha: ${g}` : undefined;
  }
  return "mentioned in its gotchas";
}
