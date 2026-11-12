import { useMemo, useState } from "react";
import { Link } from "react-router-dom";
import { allLessons, modules } from "../content/loader";
import { indexLessons, searchEntries } from "../content/search";
import type { Hit } from "../content/search";
import { allCards } from "../warmup/bank";
import { counts, SESSION_SIZE } from "../warmup/session";
import { localDate } from "../state/streak";
import type { Lesson, Module } from "../content/types";
import { runner } from "../runner";
import { wasmProblem } from "../content/wasmCompat";
import { catalog } from "../badges/catalog";
import { isModuleComplete } from "../badges/engine";
import type { LessonProgress, Progress } from "../state/model";
import { useAllProgress } from "../state/store";
import { weakSpotsFor } from "../warmup/weakView";
import { dueResolves } from "../state/resolves";

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

function Row({ lesson, p, hint, module, resolveDue }: { lesson: Lesson; p: LessonProgress | undefined; hint?: string; module?: string; resolveDue?: boolean }) {
  const kind = KIND_LABEL[lesson.meta.type];
  const label = lesson.meta.type === "lesson" ? `${lesson.order}. ${lesson.meta.title}` : lesson.meta.title;
  if (!isRunnable(lesson)) {
    return (
      <li className="locked" title={`Not available in this version: ${unrunnableReason(lesson)}. Run ${FULL_RUNNER} to try it.`}>
        <span className="row-main">
          <span>{label}</span>
          {(module || hint) && (
            <span className="muted small row-sub">
              {module}
              {module && hint ? " · " : ""}
              {hint}
            </span>
          )}
        </span>
        <span className="badge">Server needed</span>
      </li>
    );
  }
  return (
    <li className={lesson.meta.type === "test-out" ? "test-out" : undefined}>
      <span className="row-main">
        <Link to={`/lesson/${lesson.meta.id}`}>{label}</Link>
        {(module || hint) && (
          <span className="muted small row-sub">
            {module}
            {module && hint ? " · " : ""}
            {hint}
          </span>
        )}
      </span>
      {kind && <span className="badge kind">{kind}</span>}
      {resolveDue && (
        <Link className="badge due-chip" to={`/lesson/${lesson.meta.id}?resolve=1`} aria-label={`Re-solve ${lesson.meta.title}, due`}>
          re-solve due
        </Link>
      )}
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

interface Filters {
  incomplete: boolean;
  testOuts: boolean;
  challenges: boolean;
  runnable: boolean;
}
const NO_FILTERS: Filters = { incomplete: false, testOuts: false, challenges: false, runnable: false };
const lessonById = new Map(allLessons.map((l) => [l.meta.id, l]));
const anyUnrunnable = allLessons.some((l) => !isRunnable(l));

/** The search box and filter chips; `results` is null while nothing is being searched or filtered. */
function SearchBar({ query, setQuery, filters, setFilters, count }: { query: string; setQuery(q: string): void; filters: Filters; setFilters(f: Filters): void; count: number | null }) {
  const chip = (key: keyof Filters, label: string) => (
    <label className={filters[key] ? "chip on" : "chip"}>
      <input type="checkbox" checked={filters[key]} onChange={(e) => setFilters({ ...filters, [key]: e.target.checked })} /> {label}
    </label>
  );
  const active = query.trim() !== "" || Object.values(filters).some(Boolean);
  return (
    <section className="search" role="search" aria-label="Search the course">
      <input
        type="search"
        value={query}
        onChange={(e) => setQuery(e.target.value)}
        placeholder="Search lessons, concepts and gotchas (try: slicing, unique_ptr, overflow)"
        aria-label="Search lessons, concepts and gotchas"
      />
      <div className="chips">
        {chip("incomplete", "Not complete")}
        {chip("testOuts", "Test-outs")}
        {chip("challenges", "Challenges and debugging")}
        {anyUnrunnable && chip("runnable", "Runs in this build")}
        {active && (
          <button type="button" className="link" onClick={() => { setQuery(""); setFilters(NO_FILTERS); }}>
            Clear
          </button>
        )}
      </div>
      {count !== null && (
        <p className="muted small" role="status" aria-live="polite">
          {count === 0 ? "No lessons match." : `${count} lesson${count === 1 ? "" : "s"}`}
        </p>
      )}
    </section>
  );
}

/** A card inviting the learner to today's warm-up, when something is due or new. */
function WarmupCard({ progress }: { progress: Progress }) {
  const completed = new Set(Object.entries(progress.lessons).filter(([, p]) => p.status === "complete").map(([id]) => id));
  const { due, fresh } = counts({ cards: allCards, reviews: progress.reviews, completed, today: localDate(new Date()) });
  if (due + fresh === 0) return null;
  const n = Math.min(SESSION_SIZE, due + fresh);
  return (
    <section className="warmup-card" aria-label="Warm-up">
      <div>
        <div className="muted small">Warm-up</div>
        <strong>{n} card{n === 1 ? "" : "s"} ready</strong>{" "}
        <span className="muted small">
          {due > 0 ? `${due} to review` : "all new"}
          {due > 0 && fresh > 0 ? `, ${fresh} new` : ""}
        </span>
      </div>
      <Link className="btn" to="/warmup">
        Start
      </Link>
    </section>
  );
}

/** The lessons and topics the learner keeps missing, with one click to practise them. Hidden when there are none. */
function WeakSpotsPanel({ progress }: { progress: Progress }) {
  const spots = useMemo(() => weakSpotsFor(progress), [progress]);
  if (spots.length === 0) return null;
  const shown = spots.slice(0, 6);
  const n = progress.settings.weakClear;
  return (
    <section className="weak-card" aria-labelledby="weak-h">
      <div className="weak-head">
        <div>
          <h2 id="weak-h">Weak spots</h2>
          <p className="muted small">
            {spots.length} to work on. Each clears after {n} right answer{n === 1 ? "" : "s"} in a row.
          </p>
        </div>
        <Link className="btn primary" to="/warmup?weak=all">
          Practice these
        </Link>
      </div>
      <ul className="weak-list">
        {shown.map((s) => (
          <li key={s.unit}>
            <div className="weak-main">
              <strong>{s.title}</strong> <span className="badge">{s.kind === "lesson" ? "lesson" : "topic"}</span>
              <div className="muted small">{s.reasons.join(" · ")}</div>
            </div>
            <span className="muted small" aria-label={`${s.run} of ${s.need} right in a row`}>
              {s.run}/{s.need}
            </span>
            {s.cards > 0 ? (
              <Link className="btn" to={`/warmup?weak=${encodeURIComponent(s.unit)}`} aria-label={`Practice ${s.title}`}>
                Practice
              </Link>
            ) : (
              <Link className="btn" to={`/lesson/${s.lessonId}?resolve=1`} aria-label={`Re-solve ${s.title}`}>
                Re-solve
              </Link>
            )}
            {s.lessonId && s.cards > 0 && (
              <Link className="link" to={`/lesson/${s.lessonId}`}>
                Open lesson
              </Link>
            )}
          </li>
        ))}
      </ul>
      {spots.length > shown.length && <p className="muted small">and {spots.length - shown.length} more</p>}
    </section>
  );
}

export function Home() {
  const progress = useAllProgress();
  const [query, setQuery] = useState("");
  const [filters, setFilters] = useState<Filters>(NO_FILTERS);
  const index = useMemo(() => indexLessons(modules), []);
  const dueIds = useMemo(() => new Set(dueResolves(progress, localDate(new Date())).map((d) => d.lessonId)), [progress]);
  const searching = query.trim() !== "" || Object.values(filters).some(Boolean);
  const results: Hit[] | null = useMemo(() => {
    if (!searching) return null;
    return searchEntries(index, query).filter(({ entry }) => {
      const l = lessonById.get(entry.id);
      if (!l) return false;
      if (filters.incomplete && progress.lessons[entry.id]?.status === "complete") return false;
      if (filters.testOuts || filters.challenges) {
        const isTestOut = l.meta.type === "test-out";
        const isChallenge = ["challenge", "capstone", "debug"].includes(l.meta.type);
        if (!((filters.testOuts && isTestOut) || (filters.challenges && isChallenge))) return false;
      }
      if (filters.runnable && !isRunnable(l)) return false;
      return true;
    });
  }, [searching, query, filters, index, progress]);
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
            {testOut && <Row lesson={testOut} p={progress.lessons[testOut.meta.id]} resolveDue={dueIds.has(testOut.meta.id)} />}
            {regular.map((l) => (
              <Row key={l.meta.id} lesson={l} p={progress.lessons[l.meta.id]} resolveDue={dueIds.has(l.meta.id)} />
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
      <WeakSpotsPanel progress={progress} />
      <WarmupCard progress={progress} />
      <SearchBar query={query} setQuery={setQuery} filters={filters} setFilters={setFilters} count={results ? results.length : null} />
      {results && (
        <ul className="lesson-list results-list" aria-label="Search results">
          {results.map(({ entry, via }) => (
            <Row key={entry.id} lesson={lessonById.get(entry.id)!} p={progress.lessons[entry.id]} module={entry.moduleTitle} hint={via} resolveDue={dueIds.has(entry.id)} />
          ))}
        </ul>
      )}
      {!results && core.map(renderModule)}
      {!results && domains.length > 0 && (
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
