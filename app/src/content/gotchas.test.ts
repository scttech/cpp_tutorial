import { describe, expect, it } from "vitest";
import { gotchaEntries, humanize, parseDemo } from "./gotchas";

describe("humanize", () => {
  it("turns an id into a sentence-case title", () => {
    expect(humanize("return-reference-to-local")).toBe("Return reference to local");
    expect(humanize("int_overflow")).toBe("Int overflow");
    expect(humanize("")).toBe("");
  });
});

describe("gotchaEntries", () => {
  const yaml = `
array-decay:
  title: "An array decays to a pointer"
  text: "Passed to a function, an array becomes a pointer."
no-text:
  title: "Only a title"
extra-unlisted:
  title: "Not in the lesson"
  text: "Never shown."
`;
  it("follows the order of lesson.yaml, not of gotchas.yaml", () => {
    const g = gotchaEntries(["no-text", "array-decay"], yaml);
    expect(g.map((x) => x.id)).toEqual(["no-text", "array-decay"]);
  });
  it("uses title and text, and never shows an entry the lesson does not list", () => {
    const g = gotchaEntries(["array-decay", "no-text"], yaml);
    expect(g[0]).toEqual({ id: "array-decay", title: "An array decays to a pointer", text: "Passed to a function, an array becomes a pointer." });
    expect(g[1]).toEqual({ id: "no-text", title: "Only a title" });
  });
  it("falls back to a humanized id when there is no entry yet", () => {
    expect(gotchaEntries(["missing-one"], yaml)).toEqual([{ id: "missing-one", title: "Missing one" }]);
    expect(gotchaEntries(["missing-one"], undefined)).toEqual([{ id: "missing-one", title: "Missing one" }]);
  });
  it("survives malformed yaml and non-object documents", () => {
    expect(gotchaEntries(["a"], "{ not: [valid")).toEqual([{ id: "a", title: "A" }]);
    expect(gotchaEntries(["a"], "- just\n- a list")).toEqual([{ id: "a", title: "A" }]);
    expect(gotchaEntries(["a"], "a: 5")).toEqual([{ id: "a", title: "A" }]);
  });
  it("a lesson without gotchas has none", () => {
    expect(gotchaEntries(undefined, yaml)).toEqual([]);
  });
});

describe("demos", () => {
  const yaml = [
    "array-decay:",
    '  title: "An array decays to a pointer"',
    '  text: "Passed to a function, an array becomes a pointer."',
    "  demo:",
    "    sanitize: true",
    "    server: true",
    "    bad:",
    "      code: |",
    "        int main() { return 0; }",
    '      reports: "AddressSanitizer"',
    "    fix:",
    '      code: "int main() {}\\n"',
    '      out: "ok\\n"',
    "half:",
    '  title: "Only one program"',
    "  demo:",
    "    bad:",
    '      code: "int main() {}"',
    "",
  ].join("\n");
  it("reads both programs, the expectation fields and the sanitize flag", () => {
    const d = gotchaEntries(["array-decay"], yaml)[0]?.demo;
    expect(d?.sanitize).toBe(true);
    expect(d?.server).toBe(true);
    expect(d?.bad).toEqual({ code: "int main() { return 0; }\n", reports: "AddressSanitizer" });
    expect(d?.fix.out).toBe("ok\n");
  });
  it("ignores a demo that does not have both programs", () => {
    expect(gotchaEntries(["half"], yaml)[0]?.demo).toBeUndefined();
    expect(parseDemo({ bad: { code: "x" }, fix: { code: "  " } })).toBeUndefined();
    expect(parseDemo("nope")).toBeUndefined();
    expect(parseDemo({ bad: { code: "a" }, fix: { code: "b" } })).toEqual({ bad: { code: "a" }, fix: { code: "b" }, sanitize: false, server: false });
  });
  it("an entry without a demo has none", () => {
    expect(gotchaEntries(["a"], "a:\n  title: T\n  text: x")[0]).toEqual({ id: "a", title: "T", text: "x" });
  });
});

