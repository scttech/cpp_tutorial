import { useCallback, useEffect, useMemo, useRef, useState } from "react";
import { Link, useParams, useSearchParams } from "react-router-dom";
import { CodeEditor } from "../components/CodeEditor";
import { CompareView } from "../components/CompareView";
import { GotchaList } from "../components/GotchaList";
import { HintLadder } from "../components/HintLadder";
import { TutorPanel } from "../components/TutorPanel";
import { Tabs, panelProps } from "../components/Tabs";
import type { TabDef } from "../components/Tabs";
import type { TutorRequest } from "../components/TutorPanel";
import { useTutorSettings } from "../tutor/settings";
import { Markdown } from "../components/Markdown";
import { ResultsPanel } from "../components/ResultsPanel";
import { getLesson } from "../content/loader";
import { withFriendly } from "../diagnostics/friendly";
import type { Lesson } from "../content/types";
import { RunnerError, runner } from "../runner";
import type { CompileDiagnostic, RunResult, Tier } from "../runner";
import { countCodeLines } from "../state/recorder";
import { getState, update, recordResolve, recordRun, revealHint, revealSolution, useAllProgress, useProgress } from "../state/store";
import { describeInterval, isClean, isResolveDue, passedAll, resolveFor } from "../state/resolves";
import type { ResolveOutcome } from "../state/resolves";
import { localDate } from "../state/streak";
import { isRunnable, unrunnableReason } from "./Home";
import { useToolchain } from "../runner/useToolchain";

const TIER_ORDER: Tier[] = ["works", "robust", "fast", "clean"];

export function LessonPage() {
  const { id = "" } = useParams();
  const wantsResolve = useSearchParams()[0].get("resolve") === "1";
  const complete = useProgress(id).status === "complete";
  const lesson = getLesson(id);
  if (!lesson) {
    return (
      <main className="page">
        <p>
          Lesson not found. <Link to="/">Back to the course map</Link>
        </p>
      </main>
    );
  }
  // key= resets all editor/result state when navigating between lessons
  // A re-solve is only for a lesson that has been passed.
  const resolving = wantsResolve && complete;
  return <LessonView key={`${lesson.meta.id}:${resolving}`} lesson={lesson} resolving={resolving} />;
}

type LeftTab = "lesson" | "fast" | "gotchas" | "hints" | "tutor";

/** Tabs follow lesson.yaml: editable files first, then read-only ones, then anything else. */
function orderFiles(lesson: Lesson, files: Record<string, string>): Record<string, string> {
  const wanted = [...lesson.meta.files.editable, ...(lesson.meta.files.readonly ?? [])];
  const names = [...wanted.filter((n) => n in files), ...Object.keys(files).filter((n) => !wanted.includes(n)).sort()];
  return Object.fromEntries(names.map((n) => [n, files[n] as string]));
}

const clock = (ms: number): string => `${Math.floor(ms / 60000)}:${String(Math.floor(ms / 1000) % 60).padStart(2, "0")}`;

function LessonView({ lesson, resolving }: { lesson: Lesson; resolving: boolean }) {
  const { meta } = lesson;
  const progress = useProgress(meta.id);
  const readonlyFiles = meta.files.readonly ?? [];
  const [files, setFiles] = useState<Record<string, string>>(() =>
    orderFiles(lesson, resolving ? { ...lesson.starter } : { ...lesson.starter, ...progress.lastCode }),
  );
  const [active, setActive] = useState(meta.files.editable[0] ?? Object.keys(lesson.starter)[0] ?? "main.cpp");
  const [stdin, setStdin] = useState("");
  const [result, setResult] = useState<RunResult | null>(null);
  const [kind, setKind] = useState<"run" | "test" | null>(null);
  const [busy, setBusy] = useState(false);
  const [error, setError] = useState<string | null>(null);
  const all = useAllProgress();
  const pace = all.settings.pace;
  // Re-solve session: nothing here touches the saved solution, the hint counter or the attempt statistics.
  const startedAt = useRef(Date.now());
  const [now, setNow] = useState(Date.now());
  const [rHints, setRHints] = useState(0);
  const [rRevealed, setRRevealed] = useState(false);
  const [outcome, setOutcome] = useState<{ out: ResolveOutcome; due: string } | null>(null);
  const outcomeRef = useRef(outcome);
  outcomeRef.current = outcome;
  const finishRef = useRef<(passed: boolean) => void>(() => undefined);
  useEffect(() => {
    if (!resolving || outcome) return;
    const t = setInterval(() => setNow(Date.now()), 1000);
    return () => clearInterval(t);
  }, [resolving, outcome]);
  const resolveEntry = resolveFor(all, meta.id);
  const dueForResolve = !resolving && resolveEntry !== null && isResolveDue(resolveEntry, localDate(new Date()));
  // "Fast" pace (experienced programmers) opens the one-screen summary first
  const [leftTab, setLeftTab] = useState<LeftTab>(pace === "fast" && lesson.fastMd ? "fast" : "lesson");
  const [solution, setSolution] = useState<Record<string, string> | null>(null);
  const [comparing, setComparing] = useState(false);
  const [jump, setJump] = useState<{ file: string; line: number; column: number; nonce: number } | null>(null);
  const abortRef = useRef<AbortController | null>(null);
  const toolchain = useToolchain();
  const hasTutorKey = useTutorSettings().apiKey !== "";
  const [tutorRequest, setTutorRequest] = useState<TutorRequest | null>(null);
  const filesRef = useRef(files);
  filesRef.current = files;

  const declared = useMemo(() => TIER_ORDER.filter((t) => meta.tiers[t]), [meta.tiers]);

  const onChange = useCallback(
    (file: string, value: string) => {
      const next = { ...filesRef.current, [file]: value };
      filesRef.current = next;
      setFiles(next);
      if (!resolving) update(meta.id, (p) => ({ ...p, lastCode: next, status: p.status === "complete" ? "complete" : "in_progress" }));
    },
    [meta.id, resolving],
  );

  const execute = useCallback(
    async (mode: "run" | "test") => {
      if (abortRef.current) return; // already running
      const ctl = new AbortController();
      abortRef.current = ctl;
      setBusy(true);
      setError(null);
      try {
        const testFiles = mode === "test" ? await lesson.loadTests() : {};
        const res = await runner.run(
          {
            mode: mode === "test" && meta.grader === "variants" ? "grade" : mode,
            userFiles: filesRef.current,
            testFiles,
            flags: meta.flags ?? ["-std=c++20", "-Wall", "-Wextra"],
            libs: meta.libs,
            analyzers: mode === "run" ? undefined : meta.analyzers,
            stdin: mode === "run" ? stdin : undefined,
            limits: meta.limits ?? {},
          },
          ctl.signal,
        );
        const shown: RunResult = { ...res, diagnostics: withFriendly(res.diagnostics) };
        setResult(shown);
        setKind(mode);
        if (!resolving) recordRun({ lessonId: meta.id, mode, declaredTiers: declared, result: shown, userLines: countCodeLines(filesRef.current, meta.files.editable) });
        else if (mode === "test" && !outcomeRef.current && passedAll(shown)) finishRef.current(true);
      } catch (e) {
        if (e instanceof DOMException && e.name === "AbortError") setError("Run cancelled.");
        else setError(e instanceof RunnerError ? e.message : `Unexpected error: ${String(e)}`);
      } finally {
        abortRef.current = null;
        setBusy(false);
      }
    },
    [lesson, meta, declared, stdin, resolving],
  );

  const onJump = (d: CompileDiagnostic) => {
    if (files[d.file] !== undefined) setActive(d.file);
    if (d.line > 0) setJump({ file: d.file, line: d.line, column: d.column, nonce: Date.now() });
  };

  const explainError = (d: CompileDiagnostic) => {
    setLeftTab("tutor");
    setTutorRequest({
      nonce: Date.now(),
      mode: "error",
      text: `Please explain this compiler message and why it happens (do not write the fix):
${d.file}:${d.line}:${d.column}: ${d.severity}: ${d.message}`,
    });
  };

  const reset = () => {
    if (!confirm("Reset your code to the starter? Your current code will be lost.")) return;
    const fresh = orderFiles(lesson, { ...lesson.starter });
    filesRef.current = fresh;
    setFiles(fresh);
    if (!resolving) update(meta.id, (p) => ({ ...p, lastCode: fresh }));
    setResult(null);
    setKind(null);
  };

  // Comparing with the reference shows the solution, so it counts as revealing it (once the lesson is passed, that costs nothing but the badges that need it unseen).
  const openCompare = async () => {
    if (comparing) return setComparing(false);
    if (!solution) {
      if (!progress.solutionRevealed && !confirm("Compare with the reference solution? This shows it, so it counts as revealing the solution (some badges require not revealing it).")) return;
      setSolution(await lesson.loadSolution());
      if (!progress.solutionRevealed) revealSolution(meta.id);
    }
    setComparing(true);
  };

  const askReview = () => {
    setLeftTab("tutor");
    setTutorRequest({ nonce: Date.now(), mode: "review", text: "Please review my solution." });
  };

  const reveal = async () => {
    const warning = resolving
      ? "Reveal the reference solution? This re-solve will then count as a miss (the lesson comes back in 3 days)."
      : "Reveal the reference solution? The lesson stays completable, but some badges require not revealing it.";
    if (!confirm(warning)) return;
    setSolution(await lesson.loadSolution());
    if (resolving) setRRevealed(true);
    else revealSolution(meta.id);
  };

  /** End the re-solve: record how it went and when the lesson is due next. Only once. */
  const finishResolve = (passed: boolean) => {
    if (outcomeRef.current) return;
    const out: ResolveOutcome = { passed, hintsUsed: rHints, solutionRevealed: rRevealed, ms: Date.now() - startedAt.current };
    recordResolve(meta.id, out);
    setOutcome({ out, due: getState().resolves[meta.id]?.due ?? "" });
  };
  finishRef.current = finishResolve;

  if (!isRunnable(lesson)) {
    return (
      <main className="page">
        <h1>{meta.title}</h1>
        <p className="banner">
          This lesson is not available in this version: {unrunnableReason(lesson)}. Run the full Docker version to try it. <Link to="/">Back</Link>
        </p>
      </main>
    );
  }

  const revealed = Math.min(resolving ? rHints : progress.hintsUsed, lesson.hints.length);
  const leftTabs: TabDef<LeftTab>[] = [
    { id: "lesson", label: "Lesson" },
    { id: "fast", label: "Fast summary" },
    ...(lesson.gotchas.length > 0 ? [{ id: "gotchas" as const, label: `Gotchas (${lesson.gotchas.length})` }] : []),
    { id: "hints", label: "Hints" },
    { id: "tutor", label: "Tutor" },
  ];

  return (
    <main className="lesson">
      <aside className="left" aria-label="Lesson material">
        <Tabs<LeftTab> prefix="left" label="Lesson material" tabs={leftTabs} value={leftTab} onChange={setLeftTab} />
        <div className="left-body" {...panelProps("left", leftTab)} tabIndex={0}>
          {leftTab === "lesson" && <Markdown>{lesson.lessonMd}</Markdown>}
          {leftTab === "fast" && <Markdown>{lesson.fastMd || "No fast summary for this lesson yet."}</Markdown>}
          {/* always mounted (just hidden) so the conversation lasts for the whole lesson visit */}
          <div hidden={leftTab !== "tutor"}>
            <TutorPanel lesson={lesson} files={files} result={result} progress={progress} request={tutorRequest} />
          </div>
          {leftTab === "gotchas" && <GotchaList gotchas={lesson.gotchas} />}
          {leftTab === "hints" && (
            <>
              <HintLadder
                hints={lesson.hints}
                revealed={revealed}
                onReveal={() => (resolving ? setRHints((n) => n + 1) : revealHint(meta.id))}
              />
              <hr />
              {solution ? (
                Object.entries(solution).map(([name, src]) => (
                  <div key={name}>
                    <h4>Reference solution: {name}</h4>
                    <pre className="console">{src}</pre>
                  </div>
                ))
              ) : (
                <button className="btn danger" onClick={() => void reveal()}>
                  Reveal solution…
                </button>
              )}
            </>
          )}
        </div>
      </aside>
      <section className="right">
        <div className="toolbar">
          <Link to="/" className="link">
            ← Course map
          </Link>
          <h1>{meta.title}</h1>
          <span className="spacer" />
          <button className="btn" onClick={() => void execute("run")} disabled={busy} title="Ctrl+Enter">
            ▶ Run
          </button>
          <button className="btn primary" onClick={() => void execute("test")} disabled={busy} title="Ctrl+Shift+Enter">
            ✔ Run Tests
          </button>
          {busy && (
            <button className="btn" onClick={() => abortRef.current?.abort()}>
              Cancel
            </button>
          )}
          <button className="btn" onClick={reset} disabled={busy}>
            Reset
          </button>
          {progress.status === "complete" && !resolving && (
            <>
              <button className="btn" type="button" aria-pressed={comparing} onClick={() => void openCompare()}>
                Compare with reference
              </button>
              <Link className="btn" to={`/lesson/${meta.id}?resolve=1`}>
                Re-solve
              </Link>
            </>
          )}
        </div>
        {dueForResolve && !comparing && (
          <p className="banner" role="status">
            This lesson is due for a re-solve: solve it again from the starter code to check that it stuck.{" "}
            <Link to={`/lesson/${meta.id}?resolve=1`}>Start the re-solve</Link>
          </p>
        )}
        {resolving && (
          <div className="resolve-bar" role="region" aria-label="Re-solve">
            {!outcome ? (
              <>
                <span>
                  <b>Re-solve</b> from the starter code. Your saved solution is not touched.
                </span>
                <span className="muted small">
                  {clock(now - startedAt.current)} · {rHints} hint{rHints === 1 ? "" : "s"}
                  {rRevealed ? " · solution shown" : ""}
                </span>
                <span className="spacer" />
                <button className="btn" type="button" onClick={() => finishResolve(false)}>
                  Give up
                </button>
                <Link className="btn" to={`/lesson/${meta.id}`}>
                  Leave
                </Link>
              </>
            ) : (
              <>
                <span role="status">
                  {isClean(outcome.out)
                    ? `Clean re-solve in ${clock(outcome.out.ms)}.`
                    : outcome.out.passed
                      ? `Passed in ${clock(outcome.out.ms)}, but ${outcome.out.solutionRevealed ? "you looked at the solution" : "you used a hint"}, so it counts as a miss.`
                      : "Counted as a miss."}{" "}
                  Next re-solve in {describeInterval(Math.max(1, Math.round((new Date(`${outcome.due}T00:00:00`).getTime() - new Date(`${localDate(new Date())}T00:00:00`).getTime()) / 86400000)))}.
                </span>
                <span className="spacer" />
                <Link className="btn primary" to="/">
                  Back to the course map
                </Link>
              </>
            )}
          </div>
        )}
        {toolchain.phase === "downloading" && (
          <p className="banner" role="status">
            Downloading the in-browser C++ compiler (about 90 MB, first visit only; your browser keeps it afterwards)
            {toolchain.progress.total > 0 && ` … ${Math.round((toolchain.progress.loaded / toolchain.progress.total) * 100)}%`}
          </p>
        )}
        {toolchain.phase === "failed" && (
          <p className="banner error" role="alert">
            Could not download the in-browser compiler: {toolchain.message}. Reload the page to try again.
          </p>
        )}
        <div className="panes">
          <div className="work">
        {comparing && solution && <div className="editor-wrap"><CompareView mine={files} reference={solution} onClose={() => setComparing(false)} onReview={hasTutorKey ? askReview : undefined} /></div>}
        <div className="editor-wrap" hidden={comparing}>
          <CodeEditor
            files={files}
            active={active}
            readonlyFiles={readonlyFiles}
            diagnostics={result?.diagnostics ?? []}
            onChange={onChange}
            onSelect={setActive}
            onRun={() => void execute("run")}
            onRunTests={() => void execute("test")}
            jumpTo={jump}
          />
        </div>
        <details className="stdin" hidden={comparing}>
          <summary>
            Program input (stdin) for <b>Run</b>
          </summary>
          <textarea value={stdin} onChange={(e) => setStdin(e.target.value)} rows={3} aria-label="Standard input" />
        </details>
          </div>
          <ResultsPanel result={result} kind={kind} busy={busy} error={error} declaredTiers={declared} onJump={onJump} onExplain={hasTutorKey ? explainError : undefined} />
        </div>
      </section>
    </main>
  );
}
