import { readFileSync } from "node:fs";
import { describe, expect, it } from "vitest";
import { DEFAULT_PREFS, resolveLayout, resolveTheme, sanitizePrefs } from "./prefs";

describe("sanitizePrefs", () => {
  it("falls back to the defaults for junk", () => {
    expect(sanitizePrefs(null)).toEqual(DEFAULT_PREFS);
    expect(sanitizePrefs("x")).toEqual(DEFAULT_PREFS);
    expect(sanitizePrefs({ theme: "neon", keys: "nano", motion: 3, textScale: "big" })).toEqual(DEFAULT_PREFS);
  });
  it("clamps numbers and keeps valid values", () => {
    const p = sanitizePrefs({ theme: "light", textScale: 400, editorFont: 2, keys: "vim", wordWrap: true, motion: "reduce", layout: "wide" });
    expect(p).toEqual({ theme: "light", textScale: 150, editorFont: 12, keys: "vim", wordWrap: true, motion: "reduce", layout: "wide" });
    expect(sanitizePrefs({ layout: "huge" }).layout).toBe("auto");
  });
  it("defaults to the dark theme", () => expect(DEFAULT_PREFS.theme).toBe("dark"));
});

describe("resolveLayout", () => {
  it("auto follows the screen width; the other choices are fixed", () => {
    expect(resolveLayout("auto", true)).toBe("wide");
    expect(resolveLayout("auto", false)).toBe("normal");
    expect(resolveLayout("wide", false)).toBe("wide");
    expect(resolveLayout("normal", true)).toBe("normal");
  });
});

describe("resolveTheme", () => {
  it("passes explicit choices through", () => expect(resolveTheme("light", { contrast: true, light: false })).toBe("light"));
  it("system prefers contrast, then light, then dark", () => {
    expect(resolveTheme("system", { contrast: true, light: true })).toBe("contrast");
    expect(resolveTheme("system", { contrast: false, light: true })).toBe("light");
    expect(resolveTheme("system", { contrast: false, light: false })).toBe("dark");
  });
});

// WCAG contrast computed from the real CSS token blocks, so a palette edit cannot silently regress.
const css = readFileSync(new URL("../index.css", import.meta.url), "utf8");

function tokens(selector: string): Record<string, string> {
  const start = css.indexOf(`${selector} {`);
  const body = css.slice(start, css.indexOf("}", start));
  return Object.fromEntries([...body.matchAll(/--([a-z0-9-]+):\s*(#[0-9a-fA-F]{6})\s*;/g)].map((m) => [m[1], m[2]]));
}

const lum = (hex: string): number => {
  const c = [1, 3, 5].map((i) => parseInt(hex.slice(i, i + 2), 16) / 255).map((v) => (v <= 0.03928 ? v / 12.92 : ((v + 0.055) / 1.055) ** 2.4));
  return 0.2126 * c[0] + 0.7152 * c[1] + 0.0722 * c[2];
};
const ratio = (a: string, b: string): number => {
  const [hi, lo] = [lum(a), lum(b)].sort((x, y) => y - x);
  return (hi + 0.05) / (lo + 0.05);
};

describe.each([
  ["dark", ":root", 4.5],
  ["light", ':root[data-theme="light"]', 4.5],
  ["contrast", ':root[data-theme="contrast"]', 7],
])("%s theme contrast", (_name, selector, textMin) => {
  const t = tokens(selector as string);
  it("body text, muted text and status colors are readable on every surface", () => {
    for (const surface of ["bg", "panel", "panel2", "code-bg"]) {
      for (const fg of ["fg", "muted", "gold", "blue", "ok", "bad", "warn"]) {
        expect(ratio(t[fg], t[surface]), `${fg} on ${surface}`).toBeGreaterThanOrEqual(textMin as number);
      }
    }
  });
  it("buttons, gold badges and the focus ring are legible", () => {
    expect(ratio(t["on-primary"], t["primary-bg"])).toBeGreaterThanOrEqual(textMin as number);
    expect(ratio(t["on-gold"], t["gold"])).toBeGreaterThanOrEqual(textMin as number);
    expect(ratio(t["focus"], t["bg"])).toBeGreaterThanOrEqual(3);
    expect(ratio(t["focus"], t["panel"])).toBeGreaterThanOrEqual(3);
  });
  it("form field borders meet 3:1", () => expect(ratio(t["field-edge"], t["code-bg"])).toBeGreaterThanOrEqual(3));
});
