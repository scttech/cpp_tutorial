// The Runner abstraction (spec section 2.2). The UI only ever talks to this interface, so the
// server-backed runner and the in-browser WASM runner are interchangeable.
// Keep in sync with runner-service/src/types.ts.

/** "testing-tools": the test frameworks (GoogleTest, Catch2) and analyzers (clang-tidy, cppcheck, clang-format) in the Docker image. */
export type Capability = "exceptions" | "threads" | "sanitizers" | "testing-tools";
export type Tier = "works" | "robust" | "fast" | "clean";

export interface RunLimits {
  wallMs: number;
  memoryMb: number;
  maxOutputBytes: number;
}

export type Lib = "gtest" | "gmock" | "catch2" | "doctest";

/** A tool that checks the learner's files after the build (Docker runner only). */
export interface AnalyzerSpec {
  tool: "clang-tidy" | "cppcheck" | "clang-format";
  files: string[];
  checks?: string;
  config?: string;
}

export interface RunRequest {
  /** "grade": the learner's own tests are run against the correct code and hidden buggy variants (Docker runner only). */
  mode: "run" | "test" | "grade";
  userFiles: Record<string, string>;
  testFiles: Record<string, string>;
  flags: string[];
  libs?: Lib[];
  analyzers?: AnalyzerSpec[];
  stdin?: string;
  limits: Partial<RunLimits>;
}

export interface CompileDiagnostic {
  severity: "error" | "warning" | "note";
  file: string;
  /** 1-based; 0 means "no line" (linker errors). */
  line: number;
  column: number;
  message: string;
  /** Plain-language explanation, added client-side from content/diagnostics/friendly.yaml. */
  friendly?: string;
  friendlyLesson?: string;
  ruleId?: string;
}

export interface TestResult {
  name: string;
  status: "pass" | "fail" | "error" | "timeout";
  message?: string;
  durationMs?: number;
  tier?: Tier;
}

/** One line of a sanitizer stack trace (only frames in the learner's own files are kept). */
export interface ReportFrame {
  function: string;
  file: string;
  line: number;
  column: number;
  user: boolean;
}

/** What a sanitizer found while the program ran (Docker runner, builds with -fsanitize only). */
export interface RuntimeReport {
  tool: "AddressSanitizer" | "LeakSanitizer" | "ThreadSanitizer" | "UndefinedBehaviorSanitizer";
  kind: string;
  /** e.g. "AddressSanitizer: heap-use-after-free": what the friendly rules match. */
  summary: string;
  access?: string;
  location?: { file: string; line: number; column: number; function: string };
  frames: ReportFrame[];
  freedAt?: ReportFrame;
  allocatedAt?: ReportFrame;
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
  /** Set when a sanitizer reported a problem. */
  report?: RuntimeReport;
  timings: { compileMs: number; runMs: number };
}

export interface WarmUpProgress {
  loaded: number;
  /** 0 when the server did not say how large the download is. */
  total: number;
}

export interface Runner {
  readonly id: string;
  readonly label: string;
  /** What this runner can execute. Lessons that need more are shown as "server needed". */
  readonly capabilities: ReadonlySet<Capability>;
  run(req: RunRequest, signal?: AbortSignal): Promise<RunResult>;
  /** Resolves true if the runner is reachable and ready. */
  health(): Promise<boolean>;
  /** Fetch whatever the runner needs ahead of the first Run (the in-browser compiler is ~90 MB). */
  warmUp?(onProgress?: (p: WarmUpProgress) => void): Promise<void>;
}

export class RunnerError extends Error {
  readonly kind: "unreachable" | "busy" | "rejected" | "failed";

  constructor(message: string, kind: "unreachable" | "busy" | "rejected" | "failed") {
    super(message);
    this.kind = kind;
  }
}
