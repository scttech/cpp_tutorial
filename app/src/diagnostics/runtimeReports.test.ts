import { readFileSync } from "node:fs";
import { describe, expect, it } from "vitest";
import { parse } from "yaml";
import { compileRuntimeRules, describeReport, fillPlaceholders } from "../../../content/diagnostics/runtime-core.mjs";
import type { RawRuntimeRule, ReportLike } from "../../../content/diagnostics/runtime-core.mjs";

const raw = parse(readFileSync(new URL("../../../content/diagnostics/runtime.yaml", import.meta.url), "utf8")) as RawRuntimeRule[];
const rules = compileRuntimeRules(raw);
const at = (file: string, line: number) => ({ file, line });

describe("runtime.yaml", () => {
  it("has unique ids, a title, text and a trigger for every rule", () => {
    expect(new Set(raw.map((r) => r.id)).size).toBe(raw.length);
    for (const r of raw) {
      expect(r.title?.trim().length, `${r.id} title`).toBeGreaterThan(3);
      expect(r.friendly?.trim().length, `${r.id} friendly`).toBeGreaterThan(30);
      expect(r.trigger, `${r.id} needs a trigger`).toBeTruthy();
    }
  });
  it("rejects an invalid regex loudly, naming the rule", () => {
    expect(() => compileRuntimeRules([{ id: "bad", match: "(", title: "t", friendly: "f", trigger: "" }])).toThrow(/rule "bad"/);
  });
});

// Summaries as the runner produces them from real sanitizer output (runtime-check.mjs re-verifies against the real sandbox).
const REAL: [string, string][] = [
  ["AddressSanitizer: heap-use-after-free", "heap-use-after-free"],
  ["AddressSanitizer: use-after-poison", "heap-use-after-free"],
  ["AddressSanitizer: stack-use-after-scope", "stack-use-after-scope"],
  ["AddressSanitizer: stack-use-after-return", "stack-use-after-return"],
  ["AddressSanitizer: heap-buffer-overflow", "heap-buffer-overflow"],
  ["AddressSanitizer: double-free", "double-free"],
  ["AddressSanitizer: alloc-dealloc-mismatch", "alloc-dealloc-mismatch"],
  ["LeakSanitizer: leak", "leak"],
  ["UndefinedBehaviorSanitizer: signed integer overflow: 2147483647 + 1 cannot be represented in type 'int'", "signed-integer-overflow"],
  ["UndefinedBehaviorSanitizer: division by zero", "division-by-zero"],
  ["UndefinedBehaviorSanitizer: shift exponent 41 is too large for 32-bit type 'int'", "bad-shift"],
  ["UndefinedBehaviorSanitizer: load of null pointer of type 'int'", "null-pointer"],
  ["UndefinedBehaviorSanitizer: member access within null pointer of type 'S'", "null-pointer"],
  ["UndefinedBehaviorSanitizer: index 5 out of bounds for type 'int[4]'", "index-out-of-bounds"],
  ["UndefinedBehaviorSanitizer: load of value 2, which is not a valid value for type 'bool'", "invalid-value"],
];

describe("describeReport", () => {
  it.each(REAL)("%s -> %s", (summary, id) => {
    expect(describeReport(rules, { summary })?.ruleId).toBe(id);
  });
  it("returns null for a report no rule covers", () => {
    expect(describeReport(rules, { summary: "ThreadSanitizer: something new" })).toBeNull();
  });
  it("fills in where the problem is, and where the memory was freed and allocated", () => {
    const r: ReportLike = { summary: "AddressSanitizer: heap-use-after-free", location: at("main.cpp", 4), freedAt: at("main.cpp", 3), allocatedAt: at("main.cpp", 2) };
    const text = describeReport(rules, r)!.text;
    expect(text).toContain("main.cpp:4");
    expect(text).toContain("freed at main.cpp:3");
    expect(text).not.toMatch(/\{\w+\}/);
  });
  it("describes the access in words, and falls back to neutral phrases when the report has no frame", () => {
    expect(fillPlaceholders("did a {access} at {where}", { summary: "x", access: "READ of size 4", location: at("a.cpp", 9) })).toBe("did a read of 4 bytes at a.cpp:9");
    expect(fillPlaceholders("{where} / {freed}", { summary: "x" })).toBe("your code / an earlier point");
  });
});
