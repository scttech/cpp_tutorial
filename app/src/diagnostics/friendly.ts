import { parse } from "yaml";
import { applyFriendly, compileRules } from "../../../content/diagnostics/friendly-core.mjs";
import type { RawFriendlyRule } from "../../../content/diagnostics/friendly-core.mjs";
import type { CompileDiagnostic } from "../runner/types";

// Rules live in content/diagnostics/friendly.yaml and are shared with the validation tooling.
const raw = import.meta.glob("../../../content/diagnostics/friendly.yaml", { query: "?raw", import: "default", eager: true }) as Record<string, string>;
const parsed = parse(Object.values(raw)[0] ?? "[]") as RawFriendlyRule[];
const rules = compileRules(parsed);

/** What a rule says, by its id (the learning stats name the problems a learner hits most). */
export const friendlyRuleInfo = (id: string): { friendly: string; lesson?: string } | undefined => {
  const r = parsed.find((x) => x.id === id);
  return r ? { friendly: r.friendly, ...(r.lesson ? { lesson: r.lesson } : {}) } : undefined;
};

/** Adds plain-language explanations to compiler diagnostics. The raw message is always kept. */
export const withFriendly = (diags: CompileDiagnostic[]): CompileDiagnostic[] => applyFriendly(rules, diags);
