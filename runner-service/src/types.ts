// Wire types. These mirror the Runner interface in the project spec (section 2.2).

export interface RunLimits {
  wallMs: number;
  memoryMb: number;
  maxOutputBytes: number;
}

export interface RunRequest {
  userFiles: Record<string, string>;
  testFiles: Record<string, string>;
  flags: string[];
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
  mode: "run" | "test";
  limits: RunLimits;
}
