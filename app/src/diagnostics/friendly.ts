import { parse } from "yaml";
import { applyFriendly, compileRules } from "../../../content/diagnostics/friendly-core.mjs";
import type { RawFriendlyRule } from "../../../content/diagnostics/friendly-core.mjs";
import type { CompileDiagnostic } from "../runner/types";

// Rules live in content/diagnostics/friendly.yaml and are shared with the validation tooling.
const raw = import.meta.glob("../../../content/diagnostics/friendly.yaml", { query: "?raw", import: "default", eager: true }) as Record<string, string>;
const rules = compileRules(parse(Object.values(raw)[0] ?? "[]") as RawFriendlyRule[]);

/** Adds plain-language explanations to compiler diagnostics. The raw message is always kept. */
export const withFriendly = (diags: CompileDiagnostic[]): CompileDiagnostic[] => applyFriendly(rules, diags);
