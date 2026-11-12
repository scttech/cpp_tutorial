import { parse } from "yaml";
import type { Demo } from "../content/gotchas";
import type { Lesson } from "../content/types";
import { hashString, shuffled } from "./rand";

// A warm-up card is a multiple-choice question. Two sources:
//  - gotcha demos: the trap program of a gotcha you have met, asking what it prints, whether it compiles, or what the sanitizer says;
//    the answers come from what the demo declares, so no content is written twice;
//  - the CS bank (content/warmups/*.yaml): general computer science (boolean logic, design patterns, complexity, ...).

export interface Card {
  id: string;
  source: "demo" | "cs";
  /** "Gotchas" for demo cards, the topic name for bank cards. */
  topic: string;
  prompt: string;
  code?: string;
  choices: string[];
  /** Show the choices as code (the output of a program). */
  mono?: boolean;
  answer: number;
  /** Markdown shown after the answer. */
  explain: string;
  /** Demo cards: the lesson that must be complete before the card is asked, and the gotcha it comes from. */
  lessonId?: string;
  gotchaId?: string;
  demo?: Demo;
}

// ---- what a sanitizer report means -------------------------------------------------------------------------------

/** Substring of a declared `reports` text, and the plain-language name of that problem. */
export const REPORT_LABELS: [string, string][] = [
  ["heap-use-after-free", "Using heap memory after it was freed"],
  ["stack-use-after-scope", "Using a variable after its scope has ended"],
  ["stack-use-after-return", "Using a local variable after its function returned"],
  ["heap-buffer-overflow", "Reading or writing past the end of a heap block"],
  ["out of bounds", "An array index outside the array"],
  ["signed integer overflow", "Signed integer overflow"],
  ["cannot be represented in type", "A value that does not fit in its type"],
  ["division by zero", "Division by zero"],
  ["null pointer", "Using a null pointer"],
  ["double-free", "Freeing the same memory twice"],
  ["alloc-dealloc-mismatch", "Freeing memory with the wrong kind of delete"],
  ["shift exponent", "Shifting by too many bits"],
  ["misaligned", "A misaligned access"],
  ["data race", "A data race between two threads"],
  ["lock-order-inversion", "Two locks taken in opposite orders (a possible deadlock)"],
  ["terminate called", "The program was terminated by std::terminate"],
];
const labelOf = (reports: string) => REPORT_LABELS.find(([k]) => reports.includes(k))?.[1];

// ---- demo cards ------------------------------------------------------------------------------------------------------

const COMPILE_CHOICES = ["It compiles, and builds without a warning", "It compiles, but the compiler warns", "It does not compile"];

/** "number" for outputs that are just numbers (one per line), "text" for the rest: wrong choices should look like the right one. */
const shapeOf = (o: string) => (o.split("\n").every((l) => /^-?[\d.]+(e[+-]?\d+)?$/.test(l.trim())) ? "number" : "text");

/** How a program's output is shown as a choice. */
export const showOutput = (out: string) => (out === "" ? "(prints nothing)" : out.replace(/\n$/, ""));

function explainDemo(title: string, text: string | undefined, extra?: string): string {
  return [`**${title}**`, text ?? "", extra ?? ""].filter((s) => s !== "").join("\n\n");
}

export function buildDemoCards(lessons: Lesson[]): Card[] {
  // every output of any demo: the pool distractors are drawn from
  const outs = new Set<string>();
  for (const l of lessons) for (const g of l.gotchas) for (const s of [g.demo?.bad, g.demo?.fix]) if (s?.out !== undefined) outs.add(showOutput(s.out));
  const allOuts = [...outs].sort();

  const cards: Card[] = [];
  for (const l of lessons) {
    for (const g of l.gotchas) {
      const d = g.demo;
      if (!d) continue;
      const base = { source: "demo" as const, topic: "Gotchas", lessonId: l.meta.id, gotchaId: g.id, demo: d, code: d.bad.code };
      const idOf = (kind: string) => `d:${l.meta.id}:${g.id}:${kind}`;

      if (d.bad.error !== undefined) {
        cards.push({
          ...base,
          id: idOf("compile"),
          prompt: "Does this program compile?",
          choices: COMPILE_CHOICES,
          answer: 2,
          explain: explainDemo(g.title, g.text, `The compiler rejects it with an error mentioning \`${d.bad.error}\`.`),
        });
      } else {
        if (d.bad.warns !== undefined) {
          cards.push({
            ...base,
            id: idOf("compile"),
            prompt: "What does the compiler say about this program (built with -Wall -Wextra)?",
            choices: COMPILE_CHOICES,
            answer: 1,
            explain: explainDemo(g.title, g.text, `It builds, with a warning mentioning \`${d.bad.warns}\`.`),
          });
        }
        if (d.bad.out !== undefined && d.bad.reports === undefined) {
          const correct = showOutput(d.bad.out);
          const fix = d.fix.out !== undefined ? showOutput(d.fix.out) : undefined;
          const lines = correct.split("\n").length;
          const shape = shapeOf(correct);
          const pool = allOuts.filter((o) => o !== correct && o !== fix);
          // wrong answers that look like the right one: the same kind of output (a number, or text), and the same number of lines first
          const sameShape = pool.filter((o) => shapeOf(o) === shape);
          const similar = shuffled(sameShape.filter((o) => o.split("\n").length === lines), hashString(idOf("predict")));
          const rest = shuffled([...sameShape.filter((o) => o.split("\n").length !== lines), ...pool.filter((o) => shapeOf(o) !== shape)], hashString(idOf("predict") + "r"));
          const picks = [...(fix !== undefined && fix !== correct ? [fix] : []), ...similar, ...rest].slice(0, 3);
          const options = [correct, ...picks];
          if (options.length < 4 && d.bad.warns === undefined) options.push("It does not compile");
          const choices = shuffled(options, hashString(idOf("predict")));
          cards.push({
            ...base,
            id: idOf("predict"),
            prompt: "What does this program print?",
            choices,
            mono: true,
            answer: choices.indexOf(correct),
            explain: explainDemo(g.title, g.text),
          });
        }
        if (d.bad.reports !== undefined) {
          const label = labelOf(d.bad.reports);
          if (label) {
            const others = REPORT_LABELS.map(([, x]) => x).filter((x) => x !== label);
            const choices = shuffled([label, ...shuffled(others, hashString(idOf("wrong"))).slice(0, 3)], hashString(idOf("wrong") + "c"));
            cards.push({
              ...base,
              id: idOf("wrong"),
              prompt: "This program has a bug. What does the sanitizer report when it runs?",
              choices,
              answer: choices.indexOf(label),
              explain: explainDemo(g.title, g.text, `The report mentions \`${d.bad.reports}\`.`),
            });
          }
        }
      }
    }
  }
  return cards;
}

// ---- the CS bank ---------------------------------------------------------------------------------------------------------

export interface BankQuestion {
  id: string;
  topic: string;
  question: string;
  code?: string;
  choices: string[];
  /** Index of the right choice in `choices` as written. */
  answer: number;
  explain: string;
  /** Keep the choices in the order written (for "all of the above" style choices). */
  ordered?: boolean;
}

const isObj = (v: unknown): v is Record<string, unknown> => typeof v === "object" && v !== null && !Array.isArray(v);

/** Problems with one bank entry (empty when fine). Used by the content validator and by the loader. */
export function checkQuestion(q: unknown): string[] {
  if (!isObj(q)) return ["not a mapping"];
  const bad: string[] = [];
  const str = (k: string, min = 1) => typeof q[k] !== "string" || (q[k] as string).trim().length < min ? bad.push(`${k} must be a string`) : undefined;
  if (typeof q.id !== "string" || !/^[a-z0-9]+(-[a-z0-9]+)*$/.test(q.id)) bad.push("id must be kebab-case");
  str("topic");
  str("question", 8);
  str("explain", 20);
  if (!Array.isArray(q.choices) || q.choices.length < 2 || q.choices.length > 5 || q.choices.some((c) => typeof c !== "string" || c.trim() === "")) bad.push("choices must be 2 to 5 non-empty strings");
  else if (new Set((q.choices as string[]).map((c) => c.trim())).size !== q.choices.length) bad.push("choices must be different from each other");
  if (typeof q.answer !== "number" || !Number.isInteger(q.answer) || !Array.isArray(q.choices) || q.answer < 0 || q.answer >= q.choices.length) bad.push("answer must be the index of a choice");
  if (q.code !== undefined && typeof q.code !== "string") bad.push("code must be a string");
  return bad;
}

/** Parse the text of one bank file. Bad entries are skipped (the validator reports them). */
export function parseBank(text: string): BankQuestion[] {
  let doc: unknown;
  try {
    doc = parse(text);
  } catch {
    return [];
  }
  if (!Array.isArray(doc)) return [];
  return doc.filter((q) => checkQuestion(q).length === 0) as BankQuestion[];
}

export function buildBankCards(questions: BankQuestion[]): Card[] {
  return questions.map((q) => {
    const order = q.ordered ? q.choices.map((_, i) => i) : shuffled(q.choices.map((_, i) => i), hashString(q.id));
    const choices = order.map((i) => q.choices[i] as string);
    return {
      id: `q:${q.id}`,
      source: "cs" as const,
      topic: q.topic,
      prompt: q.question,
      ...(q.code ? { code: q.code } : {}),
      choices,
      answer: order.indexOf(q.answer),
      explain: q.explain,
    };
  });
}
