import { Link } from "react-router-dom";
import { modules } from "../content/loader";
import type { Lesson } from "../content/types";
import { runner } from "../runner";
import { wasmProblem } from "../content/wasmCompat";
import { catalog } from "../badges/catalog";
import { isModuleComplete } from "../badges/engine";
import type { LessonProgress } from "../state/model";
import { useAllProgress } from "../state/store";

/** Why this lesson cannot run with the current runner, or null if it can. */
export function unrunnableReason(l: Lesson): string | null {
  const missing = (l.meta.requires ?? []).filter((c) => !runner.capabilities.has(c));
  if (missing.length) return `it needs ${missing.join(", ")}`;
  // The in-browser compiler differs from the Docker one; a compatibility check decides lesson by lesson.
  if (runner.id === "wasm") return wasmProblem(l.meta.id);
  return null;
}
export const isRunnable = (l: Lesson) => unrunnableReason(l) === null;
const FULL_RUNNER = "the full Docker version";

const KIND_LABEL: Partial<Record<Lesson["meta"]["type"], string>> = {
  capstone: "Capstone",
  "test-out": "Test out",
  challenge: "Challenge",
  debug: "Debug",
};

function Status({ p }: { p: LessonProgress | undefined }) {
  if (p?.status === "complete") return <span className="badge ok">✔ complete</span>;
  if (p?.status === "in_progress") return <span className="badge">in progress</span>;
  return null;
}

function Row({ lesson, p }: { lesson: Lesson; p: LessonProgress | undefined }) {
  const kind = KIND_LABEL[lesson.meta.type];
  const label = lesson.meta.type === "lesson" ? `${lesson.order}. ${lesson.meta.title}` : lesson.meta.title;
  if (!isRunnable(lesson)) {
    return (
      <li className="locked" title={`Not available in this version: ${unrunnableReason(lesson)}. Run ${FULL_RUNNER} to try it.`}>
        <span>{label}</span>
        <span className="badge">Server needed</span>
      </li>
    );
  }
  return (
    <li className={lesson.meta.type === "test-out" ? "test-out" : undefined}>
      <Link to={`/lesson/${lesson.meta.id}`}>{label}</Link>
      {kind && <span className="badge kind">{kind}</span>}
      <span className="muted small">{lesson.meta.estimatedMinutes ? `${lesson.meta.estimatedMinutes} min` : ""}</span>
      <Status p={p} />
    </li>
  );
}

export function Home() {
  const progress = useAllProgress();
  return (
    <main className="home">
      <img src={`${import.meta.env.BASE_URL}logo-banner.webp`} alt="CPP: Zero to Hero. Learn C++, build skills, become a hero." className="logo" width={960} height={640} />
      {modules.map((m) => {
        const status = (l: Lesson) => progress.lessons[l.meta.id]?.status;
        const testOut = m.lessons.find((l) => l.meta.type === "test-out");
        const regular = m.lessons.filter((l) => l.meta.type !== "test-out");
        const done = regular.filter((l) => status(l) === "complete").length;
        // A module is complete when its test-out is passed, or when every other lesson is complete.
        const testedOut = testOut ? status(testOut) === "complete" : false;
        const moduleComplete = isModuleComplete(progress, catalog, m.id);
        return (
          <section key={m.id} className={moduleComplete ? "module complete" : "module"}>
            <h2>
              {m.title}{" "}
              {moduleComplete ? (
                <span className="badge ok">✔ module complete{testedOut && done < regular.length ? " (tested out)" : ""}</span>
              ) : (
                <span className="muted small">
                  {done}/{regular.length} complete
                </span>
              )}
            </h2>
            {m.summary && <p className="muted">{m.summary}</p>}
            {m.inProgress && <p className="muted small">More lessons are on the way for this module.</p>}
            <ul className="lesson-list">
              {testOut && <Row lesson={testOut} p={progress.lessons[testOut.meta.id]} />}
              {regular.map((l) => (
                <Row key={l.meta.id} lesson={l} p={progress.lessons[l.meta.id]} />
              ))}
            </ul>
            {testOut && !moduleComplete && (
              <p className="muted small">Know this already? Pass the test-out to complete the whole module. The lessons stay available.</p>
            )}
          </section>
        );
      })}
    </main>
  );
}
