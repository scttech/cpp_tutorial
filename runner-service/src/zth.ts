import type { TestResult } from "./types.js";

const PREFIX = "@@ZTH ";
const TIERS = new Set(["works", "robust", "fast", "clean"]);

/**
 * Split harness protocol lines (spec 4.2) out of program stdout.
 * Returns the user-visible stdout and the parsed test results.
 */
export function extractTestResults(stdout: string): { stdout: string; tests: TestResult[] } {
  const tests: TestResult[] = [];
  const visible: string[] = [];
  for (const line of stdout.split("\n")) {
    if (!line.startsWith(PREFIX)) {
      visible.push(line);
      continue;
    }
    try {
      const ev = JSON.parse(line.slice(PREFIX.length)) as Record<string, unknown>;
      if (ev.event !== "test" || typeof ev.name !== "string") continue;
      const status = ev.status;
      if (status !== "pass" && status !== "fail" && status !== "error" && status !== "timeout") continue;
      const t: TestResult = { name: ev.name, status };
      if (typeof ev.msg === "string") t.message = ev.msg;
      if (typeof ev.ms === "number") t.durationMs = ev.ms;
      if (typeof ev.tier === "string" && TIERS.has(ev.tier)) t.tier = ev.tier as TestResult["tier"];
      tests.push(t);
    } catch {
      // malformed protocol line: drop it rather than leak it to the user
    }
  }
  return { stdout: visible.join("\n"), tests };
}
