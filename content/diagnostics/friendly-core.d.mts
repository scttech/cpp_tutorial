export interface RawFriendlyRule {
  id: string;
  match: string;
  fileMatch?: string;
  friendly: string;
  lesson?: string;
  trigger?: string | { files: Record<string, string>; testFiles?: Record<string, string> };
}

export interface CompiledRule {
  id: string;
  re: RegExp;
  fileRe: RegExp | null;
  friendly: string;
  lesson: string | null;
}

export interface DiagnosticLike {
  severity: "error" | "warning" | "note";
  file: string;
  line: number;
  column: number;
  message: string;
  friendly?: string;
  friendlyLesson?: string;
  ruleId?: string;
}

export function compileRules(raw: RawFriendlyRule[]): CompiledRule[];
export function matchRule(
  rules: CompiledRule[],
  diag: { file?: string; message: string },
): { ruleId: string; text: string; lesson: string | null } | null;
export function applyFriendly<T extends DiagnosticLike>(rules: CompiledRule[], diags: T[]): T[];
