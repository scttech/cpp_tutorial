import { useCallback, useMemo, useRef, useState } from "react";
import { Link, useParams } from "react-router-dom";
import { CodeEditor } from "../components/CodeEditor";
import { HintLadder } from "../components/HintLadder";
import { TutorPanel } from "../components/TutorPanel";
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
import { update, recordRun, revealHint, revealSolution, useAllProgress, useProgress } from "../state/store";
import { isRunnable, unrunnableReason } from "./Home";
import { useToolchain } from "../runner/useToolchain";

const TIER_ORDER: Tier[] = ["works", "robust", "fast", "clean"];

export function LessonPage() {
  const { id = "" } = useParams();
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
  return <LessonView key={lesson.meta.id} lesson={lesson} />;
}

type LeftTab = "lesson" | "fast" | "hints" | "tutor";

/** Tabs follow lesson.yaml: editable files first, then read-only ones, then anything else. */
function orderFiles(lesson: Lesson, files: Record<string, string>): Record<string, string> {
  const wanted = [...lesson.meta.files.editable, ...(lesson.meta.files.readonly ?? [])];
  const names = [...wanted.filter((n) => n in files), ...Object.keys(files).filter((n) => !wanted.includes(n)).sort()];
  return Object.fromEntries(names.map((n) => [n, files[n] as string]));
}

function LessonView({ lesson }: { lesson: Lesson }) {
  const { meta } = lesson;
  const progress = useProgress(meta.id);
  const readonlyFiles = meta.files.readonly ?? [];
  const [files, setFiles] = useState<Record<string, string>>(() =>
    orderFiles(lesson, { ...lesson.starter, ...progress.lastCode }),
  );
  const [active, setActive] = useState(meta.files.editable[0] ?? Object.keys(lesson.starter)[0] ?? "main.cpp");
  const [stdin, setStdin] = useState("");
  const [result, setResult] = useState<RunResult | null>(null);
  const [kind, setKind] = useState<"run" | "test" | null>(null);
  const [busy, setBusy] = useState(false);
  const [error, setError] = useState<string | null>(null);
  const pace = useAllProgress().settings.pace;
  // "Fast" pace (experienced programmers) opens the one-screen summary first
  const [leftTab, setLeftTab] = useState<LeftTab>(pace === "fast" && lesson.fastMd ? "fast" : "lesson");
  const [solution, setSolution] = useState<Record<string, string> | null>(null);
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
      update(meta.id, (p) => ({ ...p, lastCode: next, status: p.status === "complete" ? "complete" : "in_progress" }));
    },
    [meta.id],
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
        recordRun({ lessonId: meta.id, mode, declaredTiers: declared, result: shown, userLines: countCodeLines(filesRef.current, meta.files.editable) });
      } catch (e) {
        if (e instanceof DOMException && e.name === "AbortError") setError("Run cancelled.");
        else setError(e instanceof RunnerError ? e.message : `Unexpected error: ${String(e)}`);
      } finally {
        abortRef.current = null;
        setBusy(false);
      }
    },
    [lesson, meta, declared, stdin],
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
    update(meta.id, (p) => ({ ...p, lastCode: fresh }));
    setResult(null);
    setKind(null);
  };

  const reveal = async () => {
    if (!confirm("Reveal the reference solution? The lesson stays completable, but some badges require not revealing it.")) return;
    setSolution(await lesson.loadSolution());
    revealSolution(meta.id);
  };

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

  const revealed = Math.min(progress.hintsUsed, lesson.hints.length);
  const tab = (id: LeftTab, label: string) => (
    <button role="tab" aria-selected={leftTab === id} className={leftTab === id ? "tab on" : "tab"} onClick={() => setLeftTab(id)}>
      {label}
    </button>
  );

  return (
    <div className="lesson">
      <aside className="left">
        <div className="tabs" role="tablist">
          {tab("lesson", "Lesson")}
          {tab("fast", "Fast summary")}
          {tab("hints", "Hints")}
          {tab("tutor", "Tutor")}
        </div>
        <div className="left-body">
          {leftTab === "lesson" && <Markdown>{lesson.lessonMd}</Markdown>}
          {leftTab === "fast" && <Markdown>{lesson.fastMd || "No fast summary for this lesson yet."}</Markdown>}
          {/* always mounted (just hidden) so the conversation lasts for the whole lesson visit */}
          <div hidden={leftTab !== "tutor"}>
            <TutorPanel lesson={lesson} files={files} result={result} progress={progress} request={tutorRequest} />
          </div>
          {leftTab === "hints" && (
            <>
              <HintLadder
                hints={lesson.hints}
                revealed={revealed}
                onReveal={() => revealHint(meta.id)}
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
        </div>
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
        <div className="editor-wrap">
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
        <details className="stdin">
          <summary>
            Program input (stdin) for <b>Run</b>
          </summary>
          <textarea value={stdin} onChange={(e) => setStdin(e.target.value)} rows={3} aria-label="Standard input" />
        </details>
        <ResultsPanel result={result} kind={kind} busy={busy} error={error} declaredTiers={declared} onJump={onJump} onExplain={hasTutorKey ? explainError : undefined} />
      </section>
    </div>
  );
}
