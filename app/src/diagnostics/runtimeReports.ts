import { parse } from "yaml";
import { compileRuntimeRules, describeReport } from "../../../content/diagnostics/runtime-core.mjs";
import type { RawRuntimeRule } from "../../../content/diagnostics/runtime-core.mjs";
import type { RuntimeReport } from "../runner/types";

// Rules live in content/diagnostics/runtime.yaml and are shared with the validation tooling.
const raw = import.meta.glob("../../../content/diagnostics/runtime.yaml", { query: "?raw", import: "default", eager: true }) as Record<string, string>;
const rules = compileRuntimeRules(parse(Object.values(raw)[0] ?? "[]") as RawRuntimeRule[]);

export interface ExplainedReport {
  /** A few words: what happened. */
  title: string;
  /** The plain-language explanation (backticks mark code). */
  text: string;
  lesson: string | null;
  ruleId: string;
}

/** A plain-language explanation of a sanitizer report, or null if no rule covers it (the raw report is still shown). */
export const explainReport = (report: RuntimeReport): ExplainedReport | null => describeReport(rules, report);
