import { modeInfo } from "./modes";
import type { TutorMode } from "./modes";

// Output guardrail (spec 8.5): the model never sees the solution, but a reply is still checked locally
// before the learner sees it. This is defence in depth, a nudge rather than a lock.

export const MAX_BLOCK_LINES = 12;

export interface CodeBlock {
  code: string;
  lines: number;
}

/** Fenced code blocks in a markdown reply. An unterminated final fence counts (the reply may be cut off). */
export function extractCodeBlocks(text: string): CodeBlock[] {
  const out: CodeBlock[] = [];
  const lines = text.split("\n");
  let fence: string | null = null;
  let body: string[] = [];
  const close = () => {
    out.push({ code: body.join("\n"), lines: body.length });
    fence = null;
    body = [];
  };
  for (const line of lines) {
    const m = /^\s*(`{3,}|~{3,})/.exec(line);
    if (fence === null) {
      if (m) fence = m[1] as string;
    } else if (m && (m[1] as string)[0] === fence[0] && (m[1] as string).length >= fence.length && /^\s*[`~]+\s*$/.test(line)) {
      close();
    } else {
      body.push(line);
    }
  }
  if (fence !== null) close();
  return out;
}

/** A line reduced to what matters for comparison: no comments, no whitespace differences. */
function norm(line: string): string {
  return line.replace(/\/\/.*$/, "").replace(/\s+/g, " ").trim();
}

/** Lines too short or too common to say anything ("}", "return 0;", "int main() {"). */
function significant(line: string): boolean {
  return line.length >= 12 && !/^(#include|using namespace|namespace|\}|return 0;)/.test(line);
}

export interface ReferenceSet {
  /** Normalised significant lines of the reference solution that are NOT already in the starter code. */
  lines: Set<string>;
}

/** Build the local comparison set. Both inputs stay in the browser: nothing here is ever sent to the model. */
export function buildReference(solution: Record<string, string>, starter: Record<string, string>): ReferenceSet {
  const given = new Set<string>();
  for (const src of Object.values(starter)) for (const l of src.split("\n")) given.add(norm(l));
  const lines = new Set<string>();
  for (const src of Object.values(solution)) {
    for (const raw of src.split("\n")) {
      const l = norm(raw);
      if (significant(l) && !given.has(l)) lines.add(l);
    }
  }
  return { lines };
}

/** How many of a block's significant lines are lines of the reference solution. */
export function overlap(block: string, ref: ReferenceSet): { hits: number; total: number } {
  const seen = new Set<string>();
  let hits = 0;
  for (const raw of block.split("\n")) {
    const l = norm(raw);
    if (!significant(l) || seen.has(l)) continue;
    seen.add(l);
    if (ref.lines.has(l)) hits += 1;
  }
  return { hits, total: seen.size };
}

export type Verdict = { ok: true } | { ok: false; reason: "too-long" | "matches-solution"; detail: string };

/**
 * Is this reply safe to show? Modes that may not write solutions reject a code block over 12 lines, and
 * one that reproduces the reference solution (3 of its distinctive lines, or all of them when it has fewer,
 * or at least half of the block's own lines when it has several).
 */
export function checkReply(text: string, mode: TutorMode, ref: ReferenceSet | null): Verdict {
  if (!modeInfo(mode).guarded) return { ok: true };
  for (const b of extractCodeBlocks(text)) {
    if (b.lines > MAX_BLOCK_LINES) return { ok: false, reason: "too-long", detail: `a ${b.lines}-line code block` };
    if (ref) {
      const { hits, total } = overlap(b.code, ref);
      // a tiny solution has only a line or two of its own: then reproducing all of them is the leak
      const enough = Math.min(3, ref.lines.size);
      if ((enough > 0 && hits >= enough) || (total >= 2 && hits * 2 >= total)) return { ok: false, reason: "matches-solution", detail: `${hits} of ${total} lines match the reference solution` };
    }
  }
  return { ok: true };
}

export const FALLBACK_MESSAGE =
  "I can't answer that without handing you the solution, and that would rob you of the learning. Try the next mode up, open the **Hints** tab, or tell me what you've tried so far and I'll ask you a better question.";
