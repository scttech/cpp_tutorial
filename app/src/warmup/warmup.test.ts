import { describe, expect, it } from "vitest";
import { buildBankCards, buildDemoCards, checkQuestion, parseBank, REPORT_LABELS, showOutput } from "./cards";
import type { Card } from "./cards";
import { hashString, shuffled } from "./rand";
import { addDays } from "./schedule";
import { counts, pickSession } from "./session";
import type { Lesson } from "../content/types";
import type { Gotcha } from "../content/gotchas";

const demo = (bad: Record<string, unknown>, fix: Record<string, unknown> = { code: "int main(){}", out: "" }): Gotcha["demo"] =>
  ({ bad: { code: "int main(){}", ...bad }, fix, sanitize: false, server: false, thread: false }) as unknown as Gotcha["demo"];
const lesson = (id: string, gotchas: Gotcha[]): Lesson => ({ meta: { id }, gotchas }) as unknown as Lesson;
const g = (id: string, d: Gotcha["demo"]): Gotcha => ({ id, title: `Title ${id}`, text: `Text ${id}`, demo: d });

describe("rand", () => {
  it("shuffled is deterministic, keeps the items, and depends on the seed", () => {
    const a = shuffled([1, 2, 3, 4, 5, 6, 7, 8], 42);
    expect(a).toEqual(shuffled([1, 2, 3, 4, 5, 6, 7, 8], 42));
    expect([...a].sort()).toEqual([1, 2, 3, 4, 5, 6, 7, 8]);
    expect(shuffled([1, 2, 3, 4, 5, 6, 7, 8], 43)).not.toEqual(a);
    expect(hashString("abc")).toBe(hashString("abc"));
    expect(hashString("abc")).not.toBe(hashString("abd"));
  });
});

describe("buildDemoCards", () => {
  const lessons = [
    lesson("l1", [
      g("prints", demo({ out: "-6\n" }, { code: "x", out: "2\n" })),
      g("errors", demo({ error: "narrow" }, { code: "x", out: "3\n" })),
      g("warns", demo({ warns: "uninitialized", out: "0\n" }, { code: "x", out: "0\n" })),
      g("sanitizer", demo({ reports: "heap-use-after-free" }, { code: "x", out: "7\n" })),
      g("no-demo", undefined),
      g("unknown-report", demo({ reports: "some-new-report" })),
    ]),
    lesson("l2", [g("other", demo({ out: "1 2 3\n" }, { code: "x", out: "3 2 1\n" })), g("third", demo({ out: "9\n" }, { code: "x", out: "8\n" }))]),
  ];
  const cards = buildDemoCards(lessons);
  const byId = (id: string) => cards.find((c) => c.id === id) as Card;

  it("makes the right kinds of card and skips what it cannot ask", () => {
    expect(cards.map((c) => c.id).sort()).toEqual(
      ["d:l1:errors:compile", "d:l1:prints:predict", "d:l1:sanitizer:wrong", "d:l1:warns:compile", "d:l1:warns:predict", "d:l2:other:predict", "d:l2:third:predict"].sort(),
    );
  });
  it("a predict card has the true output, the fixed program's output, and distinct choices", () => {
    const c = byId("d:l1:prints:predict");
    expect(c.choices[c.answer]).toBe("-6");
    expect(c.choices).toContain("2"); // what the fixed program prints: the natural wrong answer
    expect(new Set(c.choices).size).toBe(c.choices.length);
    expect(c.choices.length).toBeGreaterThanOrEqual(3);
    expect(c.mono).toBe(true);
    expect(c.code).toBe("int main(){}");
  });
  it("a compile card has the answer fixed by what the demo declares", () => {
    const e = byId("d:l1:errors:compile");
    expect(e.choices[e.answer]).toBe("It does not compile");
    expect(e.explain).toContain("narrow");
    const w = byId("d:l1:warns:compile");
    expect(w.choices[w.answer]).toBe("It compiles, but the compiler warns");
  });
  it("a sanitizer card names the problem and offers other problems", () => {
    const c = byId("d:l1:sanitizer:wrong");
    expect(c.choices[c.answer]).toBe("Using heap memory after it was freed");
    expect(c.choices).toHaveLength(4);
    expect(new Set(c.choices).size).toBe(4);
  });
  it("shows nothing printed readably, and every report label is distinct", () => {
    expect(showOutput("")).toBe("(prints nothing)");
    expect(showOutput("a\nb\n")).toBe("a\nb");
    expect(new Set(REPORT_LABELS.map(([, l]) => l)).size).toBe(REPORT_LABELS.length);
  });
  it("is deterministic", () => {
    expect(buildDemoCards(lessons)).toEqual(cards);
  });
});

describe("the CS bank", () => {
  const q = { id: "demorgan", topic: "Boolean logic", question: "Which equals !(a && b)?", choices: ["!a && !b", "!a || !b", "a || b"], answer: 1, explain: "De Morgan: the negation of an AND is an OR of negations." };
  it("accepts a valid question and rejects broken ones", () => {
    expect(checkQuestion(q)).toEqual([]);
    expect(checkQuestion({ ...q, answer: 3 })).not.toEqual([]);
    expect(checkQuestion({ ...q, choices: ["a", "a"] })).not.toEqual([]);
    expect(checkQuestion({ ...q, choices: ["only one"] })).not.toEqual([]);
    expect(checkQuestion({ ...q, id: "Not Kebab" })).not.toEqual([]);
    expect(checkQuestion({ ...q, explain: "short" })).not.toEqual([]);
    expect(checkQuestion("text")).not.toEqual([]);
  });
  it("parses yaml and skips bad entries", () => {
    const yaml = `- id: ok-one\n  topic: T\n  question: "A question here?"\n  choices: [a, b]\n  answer: 0\n  explain: "An explanation that is long enough."\n- id: broken\n  topic: T\n`;
    expect(parseBank(yaml).map((x) => x.id)).toEqual(["ok-one"]);
    expect(parseBank("{ not: valid")).toEqual([]);
    expect(parseBank("a: 1")).toEqual([]);
  });
  it("keeps the right answer when it shuffles, and keeps the order when asked to", () => {
    const [c] = buildBankCards([q]);
    expect(c?.choices[c.answer]).toBe("!a || !b");
    expect(c?.id).toBe("q:demorgan");
    const [o] = buildBankCards([{ ...q, ordered: true }]);
    expect(o?.choices).toEqual(q.choices);
    expect(o?.answer).toBe(1);
  });
});

describe("pickSession", () => {
  const demoCard = (n: number): Card => ({ id: `d:l${n}:g${n}:predict`, source: "demo", topic: "Gotchas", prompt: "p", choices: ["a", "b"], answer: 0, explain: "e", lessonId: `l${n}`, gotchaId: `g${n}` });
  const csCard = (n: number): Card => ({ id: `q:c${n}`, source: "cs", topic: "T", prompt: "p", choices: ["a", "b"], answer: 0, explain: "e" });
  const cards = [...Array.from({ length: 8 }, (_, i) => demoCard(i)), ...Array.from({ length: 4 }, (_, i) => csCard(i))];
  const today = "2026-05-10";
  const all = new Set(cards.filter((c) => c.lessonId).map((c) => c.lessonId as string));

  it("asks only about completed lessons, but always from the bank", () => {
    const s = pickSession({ cards, reviews: {}, completed: new Set(["l0"]), today, size: 20 });
    expect(s.map((c) => c.id).sort()).toEqual(["d:l0:g0:predict", "q:c0", "q:c1", "q:c2", "q:c3"].sort());
  });
  it("gives five cards, mixing gotcha cards with one general question for every two", () => {
    const s = pickSession({ cards, reviews: {}, completed: all, today });
    expect(s).toHaveLength(5);
    expect(s.map((c) => c.source)).toEqual(["demo", "demo", "cs", "demo", "demo"]);
  });
  it("is the same for the same inputs, and changes from day to day", () => {
    const a = pickSession({ cards, reviews: {}, completed: all, today });
    expect(pickSession({ cards, reviews: {}, completed: all, today })).toEqual(a);
    expect(pickSession({ cards, reviews: {}, completed: all, today: "2026-05-11" })).not.toEqual(a);
  });
  it("puts due reviews first, most overdue first, and leaves room for new cards", () => {
    const reviews = Object.fromEntries(cards.slice(0, 6).map((c, i) => [c.id, { box: 2, due: addDays(today, -(i + 1)), seen: 1, correct: 1 }]));
    const s = pickSession({ cards, reviews, completed: all, today });
    expect(s.slice(0, 3).map((c) => c.id)).toEqual([cards[5], cards[4], cards[3]].map((c) => (c as Card).id)); // the longest overdue first
    expect(s.slice(3).every((c) => !reviews[c.id])).toBe(true); // the rest is new
  });
  it("does not ask a card that is not due yet", () => {
    const reviews = Object.fromEntries(cards.map((c) => [c.id, { box: 3, due: addDays(today, 5), seen: 2, correct: 2 }]));
    expect(pickSession({ cards, reviews, completed: all, today })).toEqual([]);
    expect(counts({ cards, reviews, completed: all, today })).toEqual({ due: 0, fresh: 0 });
  });
  it("asks about one gotcha at most once per set", () => {
    const twin: Card = { ...demoCard(0), id: "d:l0:g0:compile" };
    const s = pickSession({ cards: [demoCard(0), twin], reviews: {}, completed: new Set(["l0"]), today });
    expect(s).toHaveLength(1);
  });
  it("counts due and new cards", () => {
    const reviews = { [(cards[0] as Card).id]: { box: 2, due: today, seen: 1, correct: 1 } };
    expect(counts({ cards, reviews, completed: all, today })).toEqual({ due: 1, fresh: 11 });
  });
});
