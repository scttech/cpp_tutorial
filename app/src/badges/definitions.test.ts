import { readFileSync, readdirSync } from "node:fs";
import { describe, expect, it } from "vitest";
import { parse } from "yaml";
import { checkRule } from "./engine";
import type { BadgeDef, Rule } from "./engine";

const dir = new URL("../../../content/badges/", import.meta.url);
const defs: BadgeDef[] = readdirSync(dir)
  .filter((f) => f.endsWith(".yaml"))
  .flatMap((f) => parse(readFileSync(new URL(f, dir), "utf8")) as BadgeDef[]);

describe("content/badges/*.yaml", () => {
  it("loads a sensible number of badges", () => {
    expect(defs.length).toBeGreaterThanOrEqual(58);
  });
  it("has unique ids and complete metadata", () => {
    expect(new Set(defs.map((b) => b.id)).size).toBe(defs.length);
    for (const b of defs) {
      expect(b.name?.length, b.id).toBeGreaterThan(1);
      expect(b.description?.length, b.id).toBeGreaterThan(10);
      expect(b.icon, b.id).toBeTruthy();
      expect(["progress", "mastery", "habits", "consistency", "fun", "domain"], b.id).toContain(b.category);
    }
  });
  it("every rule is structurally valid", () => {
    for (const b of defs) expect(checkRule(b.rule), b.id).toEqual([]);
  });
  it("tiered badges (bronze/silver/gold) come in complete families", () => {
    const families = new Map<string, Set<string>>();
    for (const b of defs.filter((x) => x.tier)) families.set(b.name, (families.get(b.name) ?? new Set()).add(b.tier!));
    for (const [name, tiers] of families) expect([...tiers].sort(), name).toEqual(["bronze", "gold", "silver"]);
  });
  it("every lesson and tier a rule names exists in the course content", () => {
    const tracks = new URL("../../../content/tracks/", import.meta.url);
    const lessons = new Map<string, Record<string, boolean>>();
    const walk = (u: URL) => {
      for (const e of readdirSync(u, { withFileTypes: true })) {
        if (e.isDirectory()) {
          if (!["mutants", "solution", "tests", "starter"].includes(e.name)) walk(new URL(`${e.name}/`, u));
        } else if (e.name === "lesson.yaml") {
          const y = parse(readFileSync(new URL(e.name, u), "utf8")) as { id: string; tiers: Record<string, boolean> };
          lessons.set(y.id, y.tiers);
        }
      }
    };
    walk(tracks);
    const visit = (r: Rule, id: string) => {
      if (r.type === "lesson-complete") expect(lessons.has(r.lesson), `${id}: lesson ${r.lesson}`).toBe(true);
      if (r.type === "lesson-tier") expect(lessons.get(r.lesson)?.[r.tier], `${id}: ${r.lesson} declares tier ${r.tier}`).toBe(true);
      if (r.type === "all") for (const x of r.rules) visit(x, id);
    };
    for (const b of defs) visit(b.rule, b.id);
  });
  it("hidden badges are only in the fun category", () => {
    for (const b of defs.filter((x) => x.hidden)) expect(b.category, b.id).toBe("fun");
  });
});
