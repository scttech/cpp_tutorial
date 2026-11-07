import { readFileSync } from "node:fs";
import { describe, expect, it } from "vitest";
import { parse } from "yaml";
import { applyFriendly, compileRules, matchRule } from "../../../content/diagnostics/friendly-core.mjs";
import type { DiagnosticLike, RawFriendlyRule } from "../../../content/diagnostics/friendly-core.mjs";

const raw = parse(readFileSync(new URL("../../../content/diagnostics/friendly.yaml", import.meta.url), "utf8")) as RawFriendlyRule[];
const rules = compileRules(raw);
const d = (message: string, file = "main.cpp"): DiagnosticLike => ({ severity: "error", file, line: 1, column: 1, message });

describe("friendly.yaml", () => {
  it("has unique ids, non-empty text, and a trigger for every rule", () => {
    expect(new Set(raw.map((r) => r.id)).size).toBe(raw.length);
    for (const r of raw) {
      expect(r.friendly?.trim().length, r.id).toBeGreaterThan(10);
      expect(r.trigger, `${r.id} needs a trigger`).toBeTruthy();
    }
  });
  it("ships at least the ~30 most common errors the spec asks for", () => {
    expect(raw.length).toBeGreaterThanOrEqual(30);
  });
  it("rejects an invalid regex loudly, naming the rule", () => {
    expect(() => compileRules([{ id: "bad", match: "(", friendly: "x" }])).toThrow(/rule "bad"/);
  });
});

// Messages captured from the real Clang 18 runner. tools/validate-content/friendly-check.mjs re-verifies
// every rule against live compiler output; this table guards ordering/specificity with no runner needed.
const REAL: [string, string][] = [
  ["use of undeclared identifier 'cout'", "std-stream-undeclared"],
  ["no member named 'cout' in namespace 'std'", "std-stream-missing-include"],
  ["no member named 'sort' in namespace 'std'", "std-member-missing"],
  ["use of undeclared identifier 'std'", "std-namespace-unknown"],
  ["no template named 'map' in namespace 'std'; did you mean 'max'?", "std-template-missing"],
  ["expected '(' for function-style cast or type construction", "cascade-expected-paren"],
  ["use of undeclared identifier 'x'", "undeclared-identifier"],
  ["unknown type name 'Foo'", "unknown-type"],
  ["no member named 'y' in 'P'", "no-member"],
  ["expected ';' at end of declaration", "missing-semicolon"],
  ["expected ';' after struct", "missing-semicolon"],
  ["expected '}'", "missing-closing-brace"],
  ["extraneous closing brace ('}')", "extra-closing-brace"],
  ["expected expression", "expected-expression"],
  ["missing terminating '\"' character [-Winvalid-pp-token]", "unterminated-string"],
  ["cannot initialize a variable of type 'int' with an lvalue of type 'const char[2]'", "cannot-initialize"],
  ["cannot initialize a variable of type 'int *' with an rvalue of type 'int'", "cannot-initialize"],
  ["no viable conversion from 'int' to 'std::string' (aka 'basic_string<char>')", "no-viable-conversion"],
  ["no matching function for call to 'f'", "no-matching-function"],
  ["candidate function not viable: requires 2 arguments, but 1 was provided", "candidate-argument-count"],
  ["candidate function not viable: no known conversion from 'const char[2]' to 'double' for 1st argument", "candidate-conversion"],
  ["candidate function not viable: expects an lvalue for 1st argument", "candidate-needs-lvalue"],
  ["non-void function does not return a value [-Wreturn-type]", "missing-return"],
  ["unused variable 'x' [-Wunused-variable]", "unused-variable"],
  ["unused parameter 'a' [-Wunused-parameter]", "unused-parameter"],
  ["comparison of integers of different signs: 'int' and 'size_type' (aka 'unsigned long') [-Wsign-compare]", "signed-unsigned-compare"],
  ["'nope.hpp' file not found", "header-not-found"],
  ["redefinition of 'Circle'", "redefinition"],
  ["variable 'x' is uninitialized when used here [-Wuninitialized]", "uninitialized-variable"],
  ["type 'double' cannot be narrowed to 'int' in initializer list [-Wc++11-narrowing]", "narrowing"],
  ["cannot assign to variable 'x' with const-qualified type 'const int'", "const-assignment"],
  ["'this' argument to member function 'set' has type 'const A', but function is not marked const", "const-method"],
  ["'x' is a private member of 'A'", "private-member"],
  ["'main' must return 'int'", "main-must-return-int"],
  ["array index 5 is past the end of the array (that has type 'int[3]') [-Warray-bounds]", "array-index-out-of-bounds"],
  ["address of stack memory associated with local variable 'x' returned [-Wreturn-stack-address]", "returned-address-of-local"],
  ["undefined reference to `f(int)'", "undefined-reference"],
  ["multiple definition of `f()'; /tmp/a-8161fb.o:a.cpp:(.text+0x0): first defined here", "multiple-definition"],
  ["linker command failed with exit code 1 (use -v to see invocation)", "linker-failed"],
];

describe("matching real Clang messages", () => {
  it.each(REAL)("%s  ->  %s", (message, expected) => {
    expect(matchRule(rules, d(message))?.ruleId).toBe(expected);
  });
  it("substitutes captured groups into the explanation", () => {
    const hit = matchRule(rules, d("cannot initialize a variable of type 'int' with an lvalue of type 'const char[2]'"));
    expect(hit?.text).toContain("`const char[2]`");
    expect(hit?.text).toContain("`int`");
  });
  it("prefers the test-specific linker rule only for diagnostics in the tests' files", () => {
    const msg = "undefined reference to `mean(std::vector<double, std::allocator<double> > const&)'";
    expect(matchRule(rules, d(msg, "test.cpp"))?.ruleId).toBe("tests-need-function");
    expect(matchRule(rules, d(msg, "main.cpp"))?.ruleId).toBe("undefined-reference");
  });
  it("leaves unknown messages untouched", () => {
    const out = applyFriendly(rules, [d("some brand new diagnostic")]);
    expect(out[0]).toEqual(d("some brand new diagnostic"));
  });
  it("adds friendly text, the lesson link and the rule id to matches", () => {
    const [x] = applyFriendly(rules, [d("use of undeclared identifier 'cout'")]);
    expect(x).toMatchObject({ ruleId: "std-stream-undeclared", friendlyLesson: "core.foundations.hello-world" });
    expect(x?.friendly).toContain("std::cout");
  });
});
