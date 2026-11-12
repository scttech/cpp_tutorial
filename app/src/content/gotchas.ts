import { parse } from "yaml";

// A lesson lists its gotcha ids in lesson.yaml; gotchas.yaml (next to it) explains each one, and may add a pair of
// tiny runnable programs that show the trap and the fix:
//
//   array-decay:
//     title: "An array decays to a pointer"
//     text: "Passed to a function, an array becomes a pointer to its first element, so ..."
//     demo:
//       sanitize: false            # true: both programs run with AddressSanitizer + UBSan (the full version only)
//       server: false              # true: needs something the in-browser compiler lacks (the full version only)
//       thread: false              # true: both programs run with -pthread and ThreadSanitizer (the full version only)
//       bad:
//         code: |                  # a complete program (main.cpp)
//           ...
//         out: "5\n"               # expected stdout; instead: `error:` (a compile error containing this text),
//                                  # `reports:` (the sanitizer report contains this text), or `warns:` (a warning)
//       fix:
//         code: |
//           ...
//         out: "5\n"

/** One of the two programs. The expectation fields are for the content validator; the app only shows `code`. */
export interface Snippet {
  code: string;
  out?: string;
  error?: string;
  reports?: string;
  warns?: string;
}

export interface Demo {
  bad: Snippet;
  fix: Snippet;
  /** Run with -fsanitize=address,undefined: needs the full (Docker) version. */
  sanitize: boolean;
  /** Uses something the in-browser compiler lacks (for example `from_chars` for doubles): needs the full version too. */
  server: boolean;
  /** Run with -pthread -fsanitize=thread: needs the full (Docker) version. */
  thread: boolean;
}

export interface Gotcha {
  id: string;
  title: string;
  /** Missing while a lesson's explanations are still being written. */
  text?: string;
  demo?: Demo;
}

/** "return-reference-to-local" -> "Return reference to local" (only used when a lesson has no entry for the id yet). */
export function humanize(id: string): string {
  const s = id.replace(/[-_]+/g, " ").trim();
  return s.charAt(0).toUpperCase() + s.slice(1);
}

const isObj = (v: unknown): v is Record<string, unknown> => typeof v === "object" && v !== null && !Array.isArray(v);
const str = (v: unknown) => (typeof v === "string" ? v : undefined);

function snippet(v: unknown): Snippet | undefined {
  if (!isObj(v)) return undefined;
  const code = str(v.code);
  if (code === undefined || code.trim() === "") return undefined;
  const out: Snippet = { code };
  for (const k of ["out", "error", "reports", "warns"] as const) {
    const s = str(v[k]);
    if (s !== undefined) out[k] = s;
  }
  return out;
}

/** A demo needs both programs; anything less is ignored (the content validator reports it). */
export function parseDemo(v: unknown): Demo | undefined {
  if (!isObj(v)) return undefined;
  const bad = snippet(v.bad);
  const fix = snippet(v.fix);
  if (!bad || !fix) return undefined;
  return { bad, fix, sanitize: v.sanitize === true, server: v.server === true, thread: v.thread === true };
}

/** The lesson's gotchas, in the order lesson.yaml lists them. Tolerates a missing or malformed gotchas.yaml. */
export function gotchaEntries(ids: string[] | undefined, rawYaml: string | undefined): Gotcha[] {
  let doc: Record<string, unknown> = {};
  if (rawYaml) {
    try {
      const v = parse(rawYaml) as unknown;
      if (isObj(v)) doc = v;
    } catch {
      /* malformed: fall back to the ids alone (the content validator reports it) */
    }
  }
  return (ids ?? []).map((id) => {
    const o = isObj(doc[id]) ? (doc[id] as Record<string, unknown>) : {};
    const title = typeof o.title === "string" && o.title.trim() !== "" ? o.title.trim() : humanize(id);
    const text = typeof o.text === "string" && o.text.trim() !== "" ? o.text.trim() : undefined;
    const demo = parseDemo(o.demo);
    return { id, title, ...(text === undefined ? {} : { text }), ...(demo === undefined ? {} : { demo }) };
  });
}
