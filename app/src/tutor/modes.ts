// The tutor's help modes (spec section 8.3, plus "discuss"). Each mode says how much help it may give.

export type TutorMode = "nudge" | "concept" | "error" | "pinpoint" | "approach" | "discuss" | "review" | "quiz";

export interface ModeInfo {
  id: TutorMode;
  label: string;
  /** One line shown under the mode picker. */
  blurb: string;
  /** Instruction inserted into the system prompt. */
  rule: string;
  /** Review (and quiz) only make sense once the lesson is passed. */
  needsComplete: boolean;
  /** Replies in this mode are checked for solution code before the learner sees them. */
  guarded: boolean;
  /** Counts as "asking for an explanation" for the Good Question badge. */
  explains: boolean;
  /** Question used when the learner sends nothing. */
  defaultQuestion?: string;
}

export const MODES: ModeInfo[] = [
  {
    id: "nudge",
    label: "Nudge",
    blurb: "A guiding question or a pointer to where to look. No code.",
    rule: "NUDGE: ask one focused guiding question, or point at where to look. Write no code at all.",
    needsComplete: false,
    guarded: true,
    explains: false,
    defaultQuestion: "I'm stuck. Can you nudge me in the right direction?",
  },
  {
    id: "concept",
    label: "Explain the concept",
    blurb: "The idea behind it, with a small example on a different problem.",
    rule: "EXPLAIN THE CONCEPT: explain the underlying idea. You may include one short example (under 10 lines) on a clearly DIFFERENT problem from the exercise. Do not apply it to the exercise.",
    needsComplete: false,
    guarded: true,
    explains: true,
  },
  {
    id: "error",
    label: "Explain my error",
    blurb: "What a compiler error or failing test means and why it happens. You make the fix.",
    rule: "EXPLAIN MY ERROR: explain in plain language what the compiler diagnostic or failing test means and why it happens. Say where to look. Do NOT write the fix or corrected code.",
    needsComplete: false,
    guarded: true,
    explains: true,
    defaultQuestion: "Can you explain what is going wrong?",
  },
  {
    id: "pinpoint",
    label: "Pinpoint",
    blurb: "Which line or area the bug is probably in, and why. No replacement code.",
    rule: "PINPOINT: identify the line or area where the bug most likely is, and why. Give no replacement code.",
    needsComplete: false,
    guarded: true,
    explains: false,
    defaultQuestion: "Where is my bug?",
  },
  {
    id: "approach",
    label: "Approach",
    blurb: "The algorithm in prose or pseudocode, never C++.",
    rule: "APPROACH: outline the algorithm or approach in prose or numbered pseudocode. Do not write C++ for the exercise.",
    needsComplete: false,
    guarded: true,
    explains: false,
    defaultQuestion: "What approach should I take?",
  },
  {
    id: "discuss",
    label: "Discuss",
    blurb: "Talk it through: trade-offs, why C++ works this way. The tutor asks as much as it tells.",
    rule:
      "DISCUSS: have an open, Socratic conversation about the ideas in this lesson: design trade-offs, ownership and lifetimes, why C++ works this way, how it compares with languages the learner knows. " +
      "Ask questions back, offer a perspective or a counter-example, and let the learner reason their way forward. " +
      "You may talk about the exercise's concepts freely, but never state the answer to the exercise, never give the algorithm step by step, and write no code for it.",
    needsComplete: false,
    guarded: true,
    explains: true,
    defaultQuestion: "Let's talk through what this exercise is really about.",
  },
  {
    id: "review",
    label: "Review (after passing)",
    blurb: "A code review of your passing solution: idioms, style, alternatives, performance.",
    rule: "REVIEW: the learner's code passes all tests. Review it: idioms, style, alternatives, performance, pitfalls. Concrete code suggestions are allowed in this mode.",
    needsComplete: true,
    guarded: false,
    explains: false,
    defaultQuestion: "Please review my solution.",
  },
  {
    id: "quiz",
    label: "Quiz me (after passing)",
    blurb: "Three quick concept-check questions on what this lesson taught.",
    rule: "QUIZ: ask exactly three short concept-check questions about the ideas in this lesson. Number them. Do not give the answers; wait for the learner to answer.",
    needsComplete: true,
    guarded: true,
    explains: false,
    defaultQuestion: "Quiz me on this lesson.",
  },
];

export const modeInfo = (id: TutorMode): ModeInfo => MODES.find((m) => m.id === id) as ModeInfo;
