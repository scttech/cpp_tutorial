// Shared by the content tools: find lessons under content/ and load everything one needs to judge them.
import { readdir, readFile, stat } from "node:fs/promises";
import { join, relative, resolve, sep } from "node:path";
import { fileURLToPath } from "node:url";
import { parse } from "yaml";

export const CONTENT_ROOT = resolve(fileURLToPath(new URL("../../content", import.meta.url)));

export async function* walk(dir) {
  for (const e of await readdir(dir, { withFileTypes: true })) {
    const p = join(dir, e.name);
    if (e.isDirectory()) yield* walk(p);
    else yield p;
  }
}

/** All files under dir as { "relative/posix/path": text }. A missing directory is an empty tree. */
export async function readTree(dir) {
  const files = {};
  try { await stat(dir); } catch { return files; }
  for await (const p of walk(dir)) files[relative(dir, p).split(sep).join("/")] = await readFile(p, "utf8");
  return files;
}

export async function findLessonDirs(root = CONTENT_ROOT) {
  const dirs = [];
  for await (const p of walk(root)) if (p.endsWith(`${sep}lesson.yaml`)) dirs.push(resolve(p, ".."));
  return dirs.sort();
}

/** Starter, solution, hidden tests and mutants of one lesson, with the readonly starter files a solution needs. */
export async function loadLesson(dir) {
  const lesson = parse(await readFile(join(dir, "lesson.yaml"), "utf8"));
  const [starter, solution, tests] = await Promise.all(["starter", "solution", "tests"].map((d) => readTree(join(dir, d))));
  const starterReadonly = Object.fromEntries((lesson.files?.readonly ?? []).filter((f) => f in starter).map((f) => [f, starter[f]]));
  const mutants = {};
  try {
    for (const m of await readdir(join(dir, "mutants"), { withFileTypes: true })) {
      if (m.isDirectory()) mutants[m.name] = await readTree(join(dir, "mutants", m.name));
    }
  } catch { /* no mutants directory */ }
  return { dir, rel: relative(CONTENT_ROOT, dir).split(sep).join("/"), lesson, starter, solution, tests, mutants, starterReadonly };
}
