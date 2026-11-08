// Generates every lesson's mutants/ directory from its mutants.json (the source of truth), or checks that they match.
//
// A MUTANT is a plausible wrong solution that the hidden tests must reject. Each lesson's mutants.json lists them:
//   [ { "name": "off-by-one-loop",                       // the directory name (kebab-case, unique in the lesson)
//       "why": "the loop stops one element early.",      // one line; written into the file as `// MUTANT: <why>`
//       "edits": [ { "file": "main.cpp",                 // optional: defaults to the lesson's first editable file
//                    "old": "i < n", "new": "i + 1 < n"  // `old` must appear EXACTLY ONCE in the file, at that point
//                  } ],
//       "extraFiles": { "EXPECT_COMPILE_FAIL": "..." }   // optional: marker files that are not part of the solution
//   } ]
// The mutant is the solution with the edits applied in order. Files that are not edited are copied unchanged, and
// the `// MUTANT:` line is put at the top of the file of the FIRST edit.
//
//   node mutants.mjs [lesson-substring]            write the mutants/ directories (removes ones no longer listed)
//   node mutants.mjs --check [lesson-substring]    write nothing; exit 1 if any directory differs from what the spec produces
import { mkdir, readFile, readdir, rm, stat, writeFile } from "node:fs/promises";
import { dirname, join } from "node:path";
import { CONTENT_ROOT, findLessonDirs, readTree } from "./lessons.mjs";
import { parse } from "yaml";

const args = process.argv.slice(2);
const check = args.includes("--check");
const filter = args.find((a) => !a.startsWith("--"));

const HEADER = "// MUTANT: ";
const NAME = /^[a-z0-9]+(-[a-z0-9]+)*$/;

/** The files of one mutant, as { path: text }. Throws a descriptive Error for a bad spec. */
function buildMutant(solution, spec, defaultFile) {
  const where = `mutant "${spec.name}"`;
  if (!NAME.test(spec.name ?? "")) throw new Error(`${where}: the name must be kebab-case`);
  if (!spec.why || /\n/.test(spec.why)) throw new Error(`${where}: "why" must be one non-empty line`);
  if (!Array.isArray(spec.edits) || spec.edits.length === 0) throw new Error(`${where}: needs at least one edit`);
  const files = { ...solution };
  let headerFile;
  for (const [i, e] of spec.edits.entries()) {
    const file = e.file ?? defaultFile;
    if (!(file in files)) throw new Error(`${where}, edit ${i + 1}: there is no solution file "${file}"`);
    if (typeof e.old !== "string" || e.old === "" || typeof e.new !== "string") throw new Error(`${where}, edit ${i + 1}: "old" and "new" must be strings and "old" must not be empty`);
    const n = files[file].split(e.old).length - 1;
    if (n !== 1) throw new Error(`${where}, edit ${i + 1}: "old" must appear exactly once in ${file} (found ${n} times): ${JSON.stringify(e.old.slice(0, 80))}`);
    files[file] = files[file].replace(e.old, () => e.new);
    headerFile ??= file;
  }
  files[headerFile] = `${HEADER}${spec.why}\n${files[headerFile]}`;
  for (const [f, text] of Object.entries(spec.extraFiles ?? {})) {
    if (f in solution) throw new Error(`${where}: extra file "${f}" collides with a solution file`);
    files[f] = text;
  }
  return files;
}

const exists = (p) => stat(p).then(() => true, () => false);

let problems = 0;
let lessons = 0;
let mutantCount = 0;
for (const dir of await findLessonDirs()) {
  if (filter && !dir.replace(/\\/g, "/").includes(filter)) continue;
  const specPath = join(dir, "mutants.json");
  const mutantsDir = join(dir, "mutants");
  const hasSpec = await exists(specPath);
  if (!hasSpec) {
    if (await exists(mutantsDir)) {
      console.log(`FAIL  ${rel(dir)}: has a mutants/ directory but no mutants.json`);
      problems++;
    }
    continue;
  }
  lessons++;
  const lesson = parse(await readFile(join(dir, "lesson.yaml"), "utf8"));
  const defaultFile = lesson.files?.editable?.[0];
  const solution = await readTree(join(dir, "solution"));
  let specs;
  try {
    specs = JSON.parse(await readFile(specPath, "utf8"));
  } catch (e) {
    console.log(`FAIL  ${rel(dir)}/mutants.json: ${e.message}`);
    problems++;
    continue;
  }
  const expected = new Map();
  try {
    for (const s of specs) {
      if (expected.has(s.name)) throw new Error(`two mutants are called "${s.name}"`);
      expected.set(s.name, buildMutant(solution, s, defaultFile));
    }
  } catch (e) {
    console.log(`FAIL  ${rel(dir)}/mutants.json: ${e.message}`);
    problems++;
    continue;
  }
  mutantCount += expected.size;

  const existing = (await exists(mutantsDir)) ? (await readdir(mutantsDir, { withFileTypes: true })).filter((d) => d.isDirectory()).map((d) => d.name) : [];
  if (check) {
    for (const [name, files] of expected) {
      const have = existing.includes(name) ? await readTree(join(mutantsDir, name)) : null;
      if (!have) {
        console.log(`FAIL  ${rel(dir)}: mutants/${name} is missing (run: npm run mutants)`);
        problems++;
      } else if (JSON.stringify(Object.entries(files).sort()) !== JSON.stringify(Object.entries(have).sort())) {
        console.log(`FAIL  ${rel(dir)}: mutants/${name} differs from mutants.json (run: npm run mutants)`);
        problems++;
      }
    }
    for (const name of existing) {
      if (!expected.has(name)) {
        console.log(`FAIL  ${rel(dir)}: mutants/${name} is not in mutants.json (add it, or run: npm run mutants to remove it)`);
        problems++;
      }
    }
  } else {
    for (const name of existing) if (!expected.has(name)) await rm(join(mutantsDir, name), { recursive: true, force: true });
    for (const [name, files] of expected) {
      await rm(join(mutantsDir, name), { recursive: true, force: true });
      for (const [f, text] of Object.entries(files)) {
        const out = join(mutantsDir, name, f);
        await mkdir(dirname(out), { recursive: true });
        await writeFile(out, text, "utf8");
      }
    }
    console.log(`ok    ${rel(dir)}  (${expected.size} mutants)`);
  }
}

function rel(dir) {
  return dir.slice(CONTENT_ROOT.length + 1).replace(/\\/g, "/");
}

if (check) console.log(problems ? `\n${problems} problem(s)` : `mutants/ matches mutants.json for ${lessons} lesson(s) (${mutantCount} mutants)`);
process.exit(problems ? 1 : 0);
