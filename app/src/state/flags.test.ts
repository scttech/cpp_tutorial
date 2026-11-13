import { describe, expect, it } from "vitest";
import type { RunResult } from "../runner/types";
import { afterExport, afterSnooze, backupStatus, REMIND_AFTER_DAYS, sanitizeBackup } from "./backup";
import { addFlag, boundCode, flagsToMarkdown, MAX_CODE, MAX_FLAGS, MAX_NOTE, summarizeResult } from "./flags";
import { emptyLesson, emptyProgress, normalize } from "./model";
import type { Progress } from "./model";

const now = new Date("2026-05-10T12:00:00Z");
const target = { type: "lesson" as const, id: "core.x" };

describe("addFlag", () => {
  it("puts the newest first, trims the note and gives each flag its own id", () => {
    let f = addFlag([], { target, note: "  first  " }, now, "a");
    f = addFlag(f, { target, note: "second" }, new Date(now.getTime() + 1), "b");
    expect(f.map((x) => x.note)).toEqual(["second", "first"]);
    expect(new Set(f.map((x) => x.id)).size).toBe(2);
    expect(f[1]).toMatchObject({ at: now.toISOString(), target });
  });
  it("keeps only a bounded number, a bounded note and bounded code", () => {
    let f = addFlag([], { target, note: "x".repeat(MAX_NOTE + 50), code: { "main.cpp": "y".repeat(MAX_CODE + 500) } }, now, "a");
    expect(f[0]!.note).toHaveLength(MAX_NOTE);
    expect(f[0]!.code!["main.cpp"]!.length).toBeLessThan(MAX_CODE + 40);
    for (let i = 0; i < MAX_FLAGS + 5; i++) f = addFlag(f, { target, note: `n${i}` }, now, String(i));
    expect(f).toHaveLength(MAX_FLAGS);
    expect(f[0]!.note).toBe(`n${MAX_FLAGS + 4}`);
  });
  it("code is spread over files in order until the allowance is used", () => {
    const out = boundCode({ "a.cpp": "a".repeat(MAX_CODE - 10), "b.cpp": "b".repeat(100), "c.cpp": "c" });
    expect(Object.keys(out)).toEqual(["a.cpp", "b.cpp"]);
  });
  it("leaves out what was not given", () => {
    expect(Object.keys(addFlag([], { target, note: "n" }, now, "a")[0]!)).toEqual(["id", "at", "target", "note"]);
  });
});

describe("summarizeResult", () => {
  const run = (over: Partial<RunResult>): RunResult => ({ compiled: true, timedOut: false, signal: null, tests: [], diagnostics: [], stderr: "", stdout: "", exitCode: 0, compileOutput: "", timings: { compileMs: 1, runMs: 1 }, ...over }) as RunResult;
  it("says there was no run", () => expect(summarizeResult(null, null)).toMatch(/No run yet/));
  it("a build failure lists the first errors", () => {
    const s = summarizeResult(run({ compiled: false, diagnostics: [{ severity: "error", file: "main.cpp", line: 3, column: 1, message: "expected ';'" }] }), "test");
    expect(s).toContain("Run Tests: did not build");
    expect(s).toContain("error main.cpp:3: expected ';'");
  });
  it("test failures are counted and named", () => {
    const s = summarizeResult(run({ tests: [{ name: "a", status: "pass" }, { name: "b", status: "fail", message: "expected 3\ngot 4" }] as RunResult["tests"] }), "test");
    expect(s).toContain("1 of 2 passed");
    expect(s).toContain("- fail: b (expected 3)");
  });
});

describe("flagsToMarkdown", () => {
  const flags = addFlag(addFlag([], { target, note: "older note", code: { "main.cpp": "int main() {}" }, result: "Run: built", context: "Docker" }, now, "a"), { target: { type: "card", id: "c1" }, note: "wrong answer" }, new Date(now.getTime() + 5000), "b");
  const md = flagsToMarkdown(flags, (t) => (t.type === "lesson" ? "Lesson X" : "Card Y"), now);
  it("is oldest first, with title, ids, note, last run and code", () => {
    expect(md.indexOf("1. Lesson X")).toBeGreaterThan(-1);
    expect(md.indexOf("1. Lesson X")).toBeLessThan(md.indexOf("2. Card Y"));
    expect(md).toContain("`core.x`");
    expect(md).toContain("older note");
    expect(md).toContain("```cpp\nint main() {}\n```");
    expect(md).toContain("Run: built");
    expect(md).toContain("Context: Docker");
    expect(md).toContain("2 flags.");
  });
  it("code that contains backtick fences does not break the document", () => {
    const f = addFlag([], { target, note: "n", code: { "a.cpp": "// ```\nint x;" } }, now, "a");
    expect(flagsToMarkdown(f, () => "T", now)).toContain("````cpp\n// ```\nint x;\n````");
  });
});

describe("normalize flags", () => {
  it("keeps good flags and drops bad ones", () => {
    const good = { id: "a", at: "t", target: { type: "lesson", id: "l" }, note: "n", code: { "m.cpp": "x", bad: 5 } };
    const n = normalize({ flags: [good, { id: 1 }, { ...good, id: "b", target: { type: "other", id: "x" } }, "junk"] });
    expect(n.flags).toEqual([{ id: "a", at: "t", target: { type: "lesson", id: "l" }, note: "n", code: { "m.cpp": "x" } }]);
    expect(normalize({}).flags).toEqual([]);
  });
});

describe("backup reminder", () => {
  const p = (lastActivity: string, firstActivity = "2026-05-01", complete = true): Progress => ({
    ...emptyProgress(),
    lessons: complete ? { a: { ...emptyLesson(), status: "complete" } } : {},
    activity: { [firstActivity]: { warmups: 0, resolves: 0, lessons: 0, runs: 1, ms: 0 }, [lastActivity]: { warmups: 0, resolves: 0, lessons: 0, runs: 1, ms: 0 } },
  });
  const never = { lastExport: null, snoozedUntil: null };

  it("does not nag a new learner, or one with nothing completed", () => {
    expect(backupStatus(emptyProgress(), never, "2026-06-01").due).toBe(false);
    expect(backupStatus(p("2026-05-20", "2026-05-01", false), never, "2026-06-01").due).toBe(false);
  });
  it("is due a week after the first activity when never exported", () => {
    expect(backupStatus(p("2026-05-06"), never, "2026-05-07").due).toBe(false);
    expect(backupStatus(p("2026-05-06"), never, "2026-05-08").due).toBe(true);
    expect(backupStatus(p("2026-05-06"), never, "2026-05-08").daysSince).toBeNull();
  });
  it("after an export, waits a week and only nags when something changed since", () => {
    const b = afterExport(never, "2026-05-10");
    expect(backupStatus(p("2026-05-12"), b, "2026-05-16").due).toBe(false);
    expect(backupStatus(p("2026-05-12"), b, "2026-05-17")).toEqual({ due: true, daysSince: 7 });
    expect(backupStatus(p("2026-05-10"), b, "2026-06-30").due).toBe(false); // nothing done since the export
  });
  it("a snooze holds it off for a week", () => {
    const b = afterSnooze(never, "2026-05-08");
    expect(b.snoozedUntil).toBe(addDaysStr("2026-05-08", REMIND_AFTER_DAYS));
    expect(backupStatus(p("2026-05-06"), b, "2026-05-14").due).toBe(false);
    expect(backupStatus(p("2026-05-06"), b, "2026-05-15").due).toBe(true);
  });
  it("sanitizes stored state", () => {
    expect(sanitizeBackup(null)).toEqual(never);
    expect(sanitizeBackup({ lastExport: "yesterday", snoozedUntil: 5 })).toEqual(never);
    expect(sanitizeBackup({ lastExport: "2026-05-10", snoozedUntil: "2026-05-17" })).toEqual({ lastExport: "2026-05-10", snoozedUntil: "2026-05-17" });
  });
});

function addDaysStr(date: string, n: number): string {
  const d = new Date(`${date}T00:00:00Z`);
  d.setUTCDate(d.getUTCDate() + n);
  return d.toISOString().slice(0, 10);
}
