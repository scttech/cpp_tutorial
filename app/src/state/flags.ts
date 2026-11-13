import type { RunResult } from "../runner/types";

// Flags: notes you leave while working through the course ("this test is wrong", "this explanation is confusing"). Each keeps what
// is needed to reproduce the problem (your code, the last result) and can be exported as one Markdown file.

export interface Flag {
  id: string;
  /** ISO time the flag was made. */
  at: string;
  /** What it is about: a lesson or a warm-up card. */
  target: { type: "lesson" | "card"; id: string };
  note: string;
  /** Your editable files at the time (lessons only, when included). */
  code?: Record<string, string>;
  /** A short description of the last run (lessons only, when included). */
  result?: string;
  /** Which runner and what kind of session. */
  context?: string;
}

export const MAX_FLAGS = 200;
export const MAX_NOTE = 4000;
export const MAX_CODE = 20_000; // characters per flag, all files together

/** The last run in a few lines: build, tests, the first errors and the end of stderr. */
export function summarizeResult(r: RunResult | null, kind: "run" | "test" | null): string {
  if (!r) return "No run yet in this session.";
  const lines: string[] = [];
  lines.push(`${kind === "test" ? "Run Tests" : "Run"}: ${r.compiled ? "built" : "did not build"}${r.timedOut ? ", timed out" : ""}${r.signal ? `, ${r.signal}` : ""}`);
  if (r.tests.length > 0) lines.push(`Tests: ${r.tests.filter((t) => t.status === "pass").length} of ${r.tests.length} passed`);
  for (const t of r.tests.filter((x) => x.status !== "pass").slice(0, 3)) lines.push(`- ${t.status}: ${t.name}${t.message ? ` (${t.message.split("\n")[0]?.slice(0, 160)})` : ""}`);
  for (const d of r.diagnostics.filter((x) => x.severity !== "note").slice(0, 3)) lines.push(`- ${d.severity} ${d.file}:${d.line}: ${d.message.slice(0, 200)}`);
  if (r.report?.summary) lines.push(`Sanitizer: ${r.report.summary}`);
  const tail = r.stderr.trim().split("\n").slice(-3).join("\n").slice(0, 400);
  if (tail) lines.push(`stderr (end): ${tail}`);
  return lines.join("\n");
}

/** Keep only what fits: a bounded number of characters of code, files in order. */
export function boundCode(files: Record<string, string>): Record<string, string> {
  const out: Record<string, string> = {};
  let left = MAX_CODE;
  for (const [name, text] of Object.entries(files)) {
    if (left <= 0) break;
    out[name] = text.length > left ? `${text.slice(0, left)}\n/* …cut… */` : text;
    left -= text.length;
  }
  return out;
}

export interface NewFlag {
  target: Flag["target"];
  note: string;
  code?: Record<string, string>;
  result?: string;
  context?: string;
}

/** The flag list with one more at the front, the oldest dropped past MAX_FLAGS. */
export function addFlag(flags: Flag[], input: NewFlag, now: Date, idSuffix = Math.random().toString(36).slice(2, 7)): Flag[] {
  const flag: Flag = {
    id: `${now.getTime().toString(36)}${idSuffix}`,
    at: now.toISOString(),
    target: input.target,
    note: input.note.trim().slice(0, MAX_NOTE),
    ...(input.code ? { code: boundCode(input.code) } : {}),
    ...(input.result ? { result: input.result.slice(0, 1200) } : {}),
    ...(input.context ? { context: input.context.slice(0, 200) } : {}),
  };
  return [flag, ...flags].slice(0, MAX_FLAGS);
}

const fence = (s: string): string => {
  const longest = Math.max(2, ...[...s.matchAll(/`+/g)].map((m) => m[0].length));
  return "`".repeat(longest + 1);
};

/** All the flags as one Markdown document, oldest first (the order you found them in). */
export function flagsToMarkdown(flags: Flag[], titleOf: (t: Flag["target"]) => string, exportedAt: Date): string {
  const out: string[] = [`# Flagged issues`, ``, `Exported ${exportedAt.toISOString().slice(0, 10)}. ${flags.length} flag${flags.length === 1 ? "" : "s"}.`, ``];
  [...flags].reverse().forEach((f, i) => {
    out.push(`## ${i + 1}. ${titleOf(f.target)}`, ``, `- ${f.target.type === "lesson" ? "Lesson" : "Warm-up card"}: \`${f.target.id}\``, `- When: ${f.at}`);
    if (f.context) out.push(`- Context: ${f.context}`);
    out.push(``, f.note || "(no note)", ``);
    if (f.result) {
      const fe = fence(f.result);
      out.push(`Last run:`, ``, `${fe}text`, f.result, fe, ``);
    }
    for (const [name, text] of Object.entries(f.code ?? {})) {
      const fe = fence(text);
      out.push(`${name}:`, ``, `${fe}cpp`, text, fe, ``);
    }
  });
  return out.join("\n");
}
