import { describe, expect, it } from "vitest";
import { indexLessons, searchEntries, stem, words } from "./search";
import type { Lesson, Module } from "./types";

const lesson = (id: string, title: string, concepts: string[], gotchas: { title: string; text?: string }[] = [], type: Lesson["meta"]["type"] = "lesson"): Lesson =>
  ({ meta: { id, title, concepts, type }, gotchas } as unknown as Lesson);
const mod = (id: string, title: string, lessons: Lesson[]): Module => ({ id, title, order: 1, lessons } as unknown as Module);

const modules = [
  mod("core.inherit", "Inheritance and Polymorphism", [
    lesson("core.inherit.basics", "Inheritance basics", ["inheritance", "access-specifiers"], [{ title: "Assigning a derived object to a base object slices it", text: "Only the base part is copied." }]),
    lesson("core.inherit.virtual", "Virtual functions", ["virtual", "override"], [{ title: "A polymorphic base needs a virtual destructor" }]),
  ]),
  mod("core.ownership", "Ownership and RAII", [
    lesson("core.ownership.unique-ptr", "unique_ptr and the rule of zero", ["unique_ptr", "rule-of-zero"], [{ title: "Prefer make_unique to unique_ptr(new T)" }]),
    lesson("core.ownership.test-out", "Test-out: leak-proof buffer", ["raii"], [], "test-out"),
  ]),
];
const entries = indexLessons(modules);
const ids = (q: string) => searchEntries(entries, q).map((h) => h.entry.id);

describe("words and stem", () => {
  it("splits on punctuation, keeps c++", () => {
    expect(words("unique_ptr and std::vector, C++20!")).toEqual(["unique", "ptr", "and", "std", "vector", "c++20"]);
  });
  it("makes slicing, slices and slice agree", () => {
    expect(new Set([stem("slicing"), stem("slices"), stem("slice")]).size).toBe(1);
    expect(stem("ptr")).toBe("ptr");
  });
});

describe("searchEntries", () => {
  it("an empty query returns every lesson in course order", () => {
    expect(ids("")).toEqual(["core.inherit.basics", "core.inherit.virtual", "core.ownership.unique-ptr", "core.ownership.test-out"]);
    expect(ids("   ")).toHaveLength(4);
  });
  it("matches the start of words in the title", () => {
    expect(ids("virt")).toEqual(["core.inherit.virtual"]);
  });
  it("needs every word of the query to match", () => {
    expect(ids("virtual functions")).toEqual(["core.inherit.virtual"]);
    expect(ids("virtual ownership")).toEqual([]);
  });
  it("finds a lesson through a gotcha, with a hint saying where", () => {
    const hit = searchEntries(entries, "slicing")[0];
    expect(hit?.entry.id).toBe("core.inherit.basics");
    expect(hit?.via).toBe("gotcha: Assigning a derived object to a base object slices it");
  });
  it("finds a lesson through its concepts, and through the words of a gotcha text", () => {
    expect(searchEntries(entries, "specifiers")[0]?.via).toBe("concept: access-specifiers");
    expect(ids("base part")).toEqual(["core.inherit.basics"]);
  });
  it("ptr finds unique_ptr, and works across punctuation", () => {
    expect(ids("ptr")).toEqual(["core.ownership.unique-ptr"]);
    expect(ids("unique_ptr")).toEqual(["core.ownership.unique-ptr"]);
  });
  it("ranks a title match above a gotcha match", () => {
    expect(ids("virtual")[0]).toBe("core.inherit.virtual");
    const r = searchEntries(entries, "base");
    expect(r.map((h) => h.entry.id)).toEqual(["core.inherit.basics", "core.inherit.virtual"]);
  });
  it("matches the module name as a last resort, and is case-insensitive", () => {
    expect(ids("RAII")).toEqual(["core.ownership.test-out", "core.ownership.unique-ptr"]);
    expect(ids("zzz")).toEqual([]);
  });
});
