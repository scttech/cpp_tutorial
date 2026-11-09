import { parse } from "yaml";
import type { HintLevel, Lesson, LessonMeta, Module } from "./types";

// Content lives in /content at the repo root (shared with the CI validator and, later, the runner).

// Light assets are bundled up front; tests and solutions are lazy chunks.
const eager = import.meta.glob(
  [
    "../../../content/tracks/**/*.{yaml,md,cpp,hpp,h,txt}",
    "!../../../content/tracks/**/tests/**",
    "!../../../content/tracks/**/solution/**",
    "!../../../content/tracks/**/mutants/**",
  ],
  { query: "?raw", import: "default", eager: true },
) as Record<string, string>;
const lazyTests = import.meta.glob("../../../content/tracks/**/tests/**/*", { query: "?raw", import: "default" }) as Record<string, () => Promise<string>>;
const lazySolutions = import.meta.glob("../../../content/tracks/**/solution/**/*", { query: "?raw", import: "default" }) as Record<string, () => Promise<string>>;

async function loadDir(group: Record<string, () => Promise<string>>, dir: string, sub: string) {
  const prefix = `${dir}/${sub}/`;
  const out: Record<string, string> = {};
  await Promise.all(
    Object.entries(group)
      .filter(([p]) => p.startsWith(prefix))
      .map(async ([p, load]) => {
        out[p.slice(prefix.length)] = await load();
      }),
  );
  return out;
}

function build(): Module[] {
  const modules = new Map<string, Module>();
  const lessonDirs = Object.keys(eager)
    .filter((p) => p.endsWith("/lesson.yaml"))
    .map((p) => p.slice(0, -"/lesson.yaml".length));

  for (const [p, text] of Object.entries(eager)) {
    if (!p.endsWith("/module.yaml")) continue;
    const m = parse(text) as { id: string; title: string; summary?: string; order?: number; inProgress?: boolean };
    modules.set(m.id, { id: m.id, title: m.title, summary: m.summary, order: m.order ?? 99, inProgress: m.inProgress === true, lessons: [] });
  }

  for (const dir of lessonDirs) {
    const meta = parse(eager[`${dir}/lesson.yaml`]!) as LessonMeta;
    const hints = (parse(eager[`${dir}/hints.yaml`] ?? "ladder: []") as { ladder: HintLevel[] }).ladder;
    const starter: Record<string, string> = {};
    for (const [p, text] of Object.entries(eager)) {
      if (p.startsWith(`${dir}/starter/`)) starter[p.slice(`${dir}/starter/`.length)] = text;
    }
    const order = Number(/\/(\d+)-[^/]+$/.exec(dir)?.[1] ?? 99);
    const lesson: Lesson = {
      meta,
      order,
      lessonMd: eager[`${dir}/lesson.md`] ?? "",
      fastMd: eager[`${dir}/fast.md`] ?? "",
      hints,
      starter,
      loadTests: () => loadDir(lazyTests, dir, "tests"),
      loadSolution: () => loadDir(lazySolutions, dir, "solution"),
    };
    const mod = modules.get(meta.module) ?? {
      id: meta.module,
      title: meta.module,
      order: 99,
      lessons: [],
    };
    mod.lessons.push(lesson);
    modules.set(meta.module, mod);
  }

  const list = [...modules.values()].sort((a, b) => a.order - b.order);
  for (const m of list) m.lessons.sort((a, b) => a.order - b.order);
  return list;
}

export const modules: Module[] = build();
export const allLessons: Lesson[] = modules.flatMap((m) => m.lessons);
export const getLesson = (id: string) => allLessons.find((l) => l.meta.id === id);
