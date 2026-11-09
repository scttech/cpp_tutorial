import type { AnalyzerSpec, Capability, Lib, Tier } from "../runner/types";

export type LessonType = "lesson" | "challenge" | "capstone" | "test-out" | "debug";

export interface LessonMeta {
  id: string;
  title: string;
  module: string;
  type: LessonType;
  estimatedMinutes?: number;
  concepts?: string[];
  prerequisites?: string[];
  files: { editable: string[]; readonly?: string[] };
  flags?: string[];
  /** Test frameworks linked into the build (also for plain Run). Lessons using them declare `requires: [testing-tools]`. */
  libs?: Lib[];
  /** Run after the build in Run Tests: clang-tidy, cppcheck, clang-format over the learner's files. */
  analyzers?: AnalyzerSpec[];
  /** "variants": Run Tests grades the learner's own tests against the correct code and hidden buggy variants (tests/grade.json). */
  grader?: "variants";
  limits?: { wallMs?: number; memoryMb?: number };
  requires?: Capability[];
  tiers: Record<Tier, boolean>;
  gotchas?: string[];
  starterBroken?: boolean;
}

export interface HintLevel {
  level: number;
  text: string;
}

export interface Lesson {
  meta: LessonMeta;
  order: number;
  lessonMd: string;
  fastMd: string;
  hints: HintLevel[];
  starter: Record<string, string>;
  /** Hidden tests / reference solution are separate chunks, fetched only when needed. */
  loadTests(): Promise<Record<string, string>>;
  loadSolution(): Promise<Record<string, string>>;
}

export interface Module {
  id: string;
  title: string;
  summary?: string;
  order: number;
  /** Content is still being written: finishing the lessons that exist does not complete the module. */
  inProgress?: boolean;
  lessons: Lesson[];
}
