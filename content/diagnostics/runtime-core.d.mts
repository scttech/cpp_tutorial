export interface RawRuntimeRule {
  id: string;
  match: string;
  title: string;
  friendly: string;
  lesson?: string;
  flags?: string[];
  trigger: string | { files: Record<string, string>; testFiles?: Record<string, string> };
}

export interface CompiledRuntimeRule {
  id: string;
  re: RegExp;
  title: string;
  friendly: string;
  lesson: string | null;
}

export interface ReportFrame {
  function: string;
  file: string;
  line: number;
  column: number;
  user: boolean;
}

/** The part of the runner's `report` the rules use. */
export interface ReportLike {
  summary: string;
  access?: string;
  location?: { file: string; line: number };
  freedAt?: { file: string; line: number };
  allocatedAt?: { file: string; line: number };
}

export function compileRuntimeRules(raw: RawRuntimeRule[]): CompiledRuntimeRule[];
export function fillPlaceholders(text: string, report: ReportLike): string;
export function describeReport(
  rules: CompiledRuntimeRule[],
  report: ReportLike,
): { ruleId: string; title: string; text: string; lesson: string | null } | null;
