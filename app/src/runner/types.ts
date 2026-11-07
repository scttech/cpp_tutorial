// The Runner abstraction (spec section 2.2). The UI only ever talks to this interface, so the
// server-backed runner and the in-browser WASM runner are interchangeable.
// Keep in sync with runner-service/src/types.ts.

export type Capability = "exceptions" | "threads" | "sanitizers";
export type Tier = "works" | "robust" | "fast" | "clean";

export interface RunLimits {
  wallMs: number;
  memoryMb: number;
  maxOutputBytes: number;
}

export interface RunRequest {
  mode: "run" | "test";
  userFiles: Record<string, string>;
  testFiles: Record<string, string>;
  flags: string[];
  stdin?: string;
  limits: Partial<RunLimits>;
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
  tier?: Tier;
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
  timings: { compileMs: number; runMs: number };
}

export interface Runner {
  readonly id: string;
  readonly label: string;
  /** What this runner can execute. Lessons that need more are shown as "server needed". */
  readonly capabilities: ReadonlySet<Capability>;
  run(req: RunRequest, signal?: AbortSignal): Promise<RunResult>;
  /** Resolves true if the runner is reachable and ready. */
  health(): Promise<boolean>;
  warmUp?(): Promise<void>;
}

export class RunnerError extends Error {
  readonly kind: "unreachable" | "busy" | "rejected" | "failed";

  constructor(message: string, kind: "unreachable" | "busy" | "rejected" | "failed") {
    super(message);
    this.kind = kind;
  }
}
