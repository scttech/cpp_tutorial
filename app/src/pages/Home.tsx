import { useState } from "react";
import { Link } from "react-router-dom";
import { modules } from "../content/loader";
import type { Lesson, Module } from "../content/types";
import { runner } from "../runner";
import { wasmProblem } from "../content/wasmCompat";
import { catalog } from "../badges/catalog";
import { isModuleComplete } from "../badges/engine";
import type { LessonProgress, Progress } from "../state/model";
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

const OPEN_KEY = "zth.home.open";

/** The modules the learner opened or closed by hand (module id to open?). Absent ids follow the default. */
function loadChoices(): Record<string, boolean> {
  try {
    const raw: unknown = JSON.parse(localStorage.getItem(OPEN_KEY) ?? "{}");
    if (typeof raw !== "object" || raw === null || Array.isArray(raw)) return {};
    return Object.fromEntries(Object.entries(raw).filter(([, v]) => typeof v === "boolean"));
  } catch {
    return {};
  }
}
function saveChoices(c: Record<string, boolean>) {
  try {
    localStorage.setItem(OPEN_KEY, JSON.stringify(c));
  } catch {
    /* storage unavailable: the choice just lasts until the page reloads */
  }
}

/** The first lesson, in course order, that the learner can run and has not completed (modules that are complete are skipped). */
export function nextLesson(progress: Progress): { moduleId: string; lesson: Lesson; started: boolean } | null {
  const started = Object.values(progress.lessons).some((p) => p.status !== "available");
  for (const m of modules) {
    if (isModuleComplete(progress, catalog, m.id)) continue;
    const lesson = m.lessons.find((l) => l.meta.type !== "test-out" && isRunnable(l) && progress.lessons[l.meta.id]?.status !== "complete");
    if (lesson) return { moduleId: m.id, lesson, started };
  }
  return null;
}

function ContinueCard({ progress }: { progress: Progress }) {
  const next = nextLesson(progress);
  if (!next) return null;
  const inProgress = progress.lessons[next.lesson.meta.id]?.status === "in_progress";
  const module = modules.find((m) => m.id === next.moduleId)!;
  return (
    <section className="continue-card" aria-label="Continue learning">
      <div>
        <div className="muted small">{next.started ? (inProgress ? "Pick up where you left off" : "Up next") : "Start here"}</div>
        <strong>{next.lesson.meta.title}</strong> <span className="muted small">in {module.title}</span>
      </div>
      <Link className="btn primary" to={`/lesson/${next.lesson.meta.id}`}>
        {next.started ? "Continue" : "Start"}
      </Link>
    </section>
  );
}

export function Home() {
  const progress = useAllProgress();
  const [choices, setChoices] = useState(loadChoices);
  // Unless the learner chose, only the module holding the next lesson is open.
  const defaultOpen = nextLesson(progress)?.moduleId;
  const toggle = (id: string, open: boolean) =>
    setChoices((c) => {
      const next = { ...c, [id]: open };
      saveChoices(next);
      return next;
    });
  const core = modules.filter((m) => (m.track ?? "core") === "core");
  const domains = modules.filter((m) => (m.track ?? "core") !== "core");
  const renderModule = (m: Module) => {
    const status = (l: Lesson) => progress.lessons[l.meta.id]?.status;
    const testOut = m.lessons.find((l) => l.meta.type === "test-out");
    const regular = m.lessons.filter((l) => l.meta.type !== "test-out");
    const done = regular.filter((l) => status(l) === "complete").length;
    // A module is complete when its test-out is passed, or when every other lesson is complete.
    const testedOut = testOut ? status(testOut) === "complete" : false;
    const moduleComplete = isModuleComplete(progress, catalog, m.id);
    const open = choices[m.id] ?? m.id === defaultOpen;
    return (
      <section key={m.id} className={moduleComplete ? "module complete" : "module"}>
        <h2>
          <button type="button" className="module-toggle" aria-expanded={open} aria-controls={`mod-${m.id}`} onClick={() => toggle(m.id, !open)}>
            <span className="chevron" aria-hidden="true">
              {open ? "▾" : "▸"}
            </span>
            <span className="module-title">{m.title}</span>
            {moduleComplete ? (
              <span className="badge ok">✔ module complete{testedOut && done < regular.length ? " (tested out)" : ""}</span>
            ) : (
              <span className="muted small">
                {done}/{regular.length} complete
              </span>
            )}
          </button>
        </h2>
        <div id={`mod-${m.id}`} hidden={!open}>
          {m.summary && <p className="muted">{m.summary}</p>}
          {m.recommends && <p className="muted small">Recommended first: {m.recommends}.</p>}
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
        </div>
      </section>
    );
  };
  return (
    <main className="home">
      <img src={`${import.meta.env.BASE_URL}logo-banner.webp`} alt="CPP: Zero to Hero. Learn C++, build skills, become a hero." className="logo" width={960} height={640} />
      <ContinueCard progress={progress} />
      {core.map(renderModule)}
      {domains.length > 0 && (
        <section className="domain-tracks" aria-label="Domain challenge tracks">
          <h2 className="track-heading">Domain challenge tracks</h2>
          <p className="muted">
            Real-world problems, each with the four tiers: Works, Robust, Fast and Clean. Every track lists the core modules it builds on; nothing is locked, so jump in when you are ready.
          </p>
          {domains.map(renderModule)}
        </section>
      )}
    </main>
  );
}
