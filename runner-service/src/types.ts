// Wire types. These mirror the Runner interface in the project spec (section 2.2).

export interface RunLimits {
  wallMs: number;
  memoryMb: number;
  maxOutputBytes: number;
}

/** Test frameworks a job can link (doctest is header-only; the others are static libraries in the sandbox image). */
export type Lib = "gtest" | "gmock" | "catch2" | "doctest";

/** A tool that checks the learner's files after the build: findings become Problems and a "clean"-tier test result. */
export interface AnalyzerSpec {
  tool: "clang-tidy" | "cppcheck" | "clang-format";
  /** The files to check (names as in userFiles). */
  files: string[];
  /** clang-tidy only: a --checks list. */
  checks?: string;
  /** clang-tidy / clang-format: a configuration file among userFiles (e.g. a .clang-tidy kept under a visible name). */
  config?: string;
}

export interface RunRequest {
  userFiles: Record<string, string>;
  testFiles: Record<string, string>;
  flags: string[];
  libs: Lib[];
  analyzers: AnalyzerSpec[];
  stdin?: string;
  limits: RunLimits;
}

export interface CompileDiagnostic {
  severity: "error" | "warning" | "note";
  file: string;
  line: number;
  column: number;
  message: string;
  friendly?: string;
}

export interface TestResult {
  name: string;
  status: "pass" | "fail" | "error" | "timeout";
  message?: string;
  durationMs?: number;
  tier?: "works" | "robust" | "fast" | "clean";
}

/** One line of a sanitizer's stack trace. `user` is true for the learner's own files. */
export interface Frame {
  function: string;
  file: string;
  line: number;
  column: number;
  user: boolean;
}

/** What a sanitizer (AddressSanitizer, LeakSanitizer, UBSan, ThreadSanitizer) found, parsed from the program's stderr. */
export interface RuntimeReport {
  tool: "AddressSanitizer" | "LeakSanitizer" | "ThreadSanitizer" | "UndefinedBehaviorSanitizer";
  /** One lowercase word or phrase: heap-use-after-free, double-free, leak, signed-integer-overflow, ... */
  kind: string;
  /** e.g. "AddressSanitizer: heap-use-after-free": what the friendly rules match against. */
  summary: string;
  /** "READ of size 4" / "WRITE of size 8" for memory errors. */
  access?: string;
  /** The first frame in the learner's code (where it happened). */
  location?: { file: string; line: number; column: number; function: string };
  /** Learner-code frames of the failing stack, innermost first (at most 5). */
  frames: Frame[];
  /** Memory errors: where the block was freed and where it was allocated (learner code only). */
  freedAt?: Frame;
  allocatedAt?: Frame;
  /** ThreadSanitizer: the OTHER access of a data race (or the second lock acquisition of a lock-order inversion): learner code only. */
  previousAt?: Frame;
  /** ThreadSanitizer data race: what the other access was, e.g. "WRITE of size 4". */
  previousAccess?: string;
}

export interface RunResult {
  compiled: boolean;
  diagnostics: CompileDiagnostic[];
  compileOutput: string;
  stdout: string;
  stderr: string;
  tests: TestResult[];
  exitCode: number | null;
  signal: string | null;
  timedOut: boolean;
  metrics?: Record<string, number>;
  /** Set when a sanitizer reported a problem (only for jobs built with -fsanitize). */
  report?: RuntimeReport;
  timings: { compileMs: number; runMs: number };
}

/** What zth-run.py inside the sandbox prints (one JSON object). */
export interface SandboxOutput {
  compiled: boolean;
  compileOutput: string;
  stdout: string;
  stderr: string;
  exitCode: number | null;
  signal: string | null;
  timedOut: boolean;
  compileMs: number;
  runMs: number;
  internalError?: string;
}

/** Anything that can execute a job; lets tests swap in a fake. */
export interface Sandbox {
  execute(job: SandboxJob): Promise<SandboxOutput>;
}

export interface SandboxJob {
  files: Record<string, string>;
  testFiles: Record<string, string>;
  flags: string[];
  stdin: string;
  mode: "run" | "test" | "grade";
  libs: Lib[];
  analyzers: AnalyzerSpec[];
  limits: RunLimits;
}
