import { useState } from "react";
import { Link } from "react-router-dom";
import { getLesson } from "../content/loader";
import { Tabs, panelProps } from "./Tabs";
import { explainReport } from "../diagnostics/runtimeReports";
import type { CompileDiagnostic, ReportFrame, RunResult, RuntimeReport, Tier, TestResult } from "../runner/types";

const TIERS: { id: Tier; label: string }[] = [
  { id: "works", label: "Works" },
  { id: "robust", label: "Robust" },
  { id: "fast", label: "Fast" },
  { id: "clean", label: "Clean" },
];

// Icon + text (never colour alone) so pass/fail is readable for colour-blind users.
const STATUS: Record<TestResult["status"], { icon: string; label: string }> = {
  pass: { icon: "✔", label: "Passed" },
  fail: { icon: "✘", label: "Failed" },
  error: { icon: "⚠", label: "Crashed" },
  timeout: { icon: "⏱", label: "Timed out" },
};

type ResultTab = "tests" | "console" | "problems";

interface Props {
  result: RunResult | null;
  kind: "run" | "test" | null;
  busy: boolean;
  error: string | null;
  declaredTiers: Tier[];
  onJump(d: CompileDiagnostic): void;
  /** Present when the AI tutor is set up: adds an "Explain this error" button to each problem. */
  onExplain?(d: CompileDiagnostic): void;
}

export function ResultsPanel({ result, kind, busy, error, declaredTiers, onJump, onExplain }: Props) {
  const [tab, setTab] = useState<ResultTab>("tests");
  const problems = result?.diagnostics.filter((d) => d.severity !== "note") ?? [];
  const nErr = problems.filter((d) => d.severity === "error").length;

  return (
    <section className="results" aria-label="Results">
      <Tabs<ResultTab>
        prefix="results"
        label="Results"
        tabs={[
          { id: "tests", label: "Tests" },
          { id: "console", label: "Console" },
          { id: "problems", label: `Problems${problems.length ? ` (${problems.length})` : ""}` },
        ]}
        value={tab}
        onChange={setTab}
      >
        {busy && <span className="spinner" role="status">Running…</span>}
        {result && !busy && <span className="timing">compile {result.timings.compileMs} ms · run {result.timings.runMs} ms</span>}
      </Tabs>
      <div className="panel-body" {...panelProps("results", tab)} tabIndex={0}>
        {error && <p className="banner error" role="alert">{error}</p>}
        {!result && !error && <p className="muted">Press <b>Run Tests</b> (Ctrl+Shift+Enter) to check your solution, or <b>Run</b> (Ctrl+Enter) to just run it.</p>}
        {result?.report && tab !== "problems" && <RuntimeCard report={result.report} raw={result.stderr} stopped={result.tests.filter((t) => t.message?.startsWith("stopped by a sanitizer")).length} />}
        {result && tab === "tests" && <Tests result={result} kind={kind} declared={declaredTiers} nErr={nErr} onStart={() => setTab("problems")} />}
        {result && tab === "console" && <Console result={result} />}
        {result && tab === "problems" && <Problems diags={result.diagnostics} onJump={onJump} onExplain={onExplain} raw={result.compileOutput} />}
      </div>
    </section>
  );
}

function Tests({ result, kind, declared, nErr, onStart }: { result: RunResult; kind: "run" | "test" | null; declared: Tier[]; nErr: number; onStart(): void }) {
  if (!result.compiled) {
    const first = result.diagnostics.find((d) => d.severity === "error");
    return (
      <>
        <p className="banner error">
          Your code did not compile ({nErr} error{nErr === 1 ? "" : "s"}). See the{" "}
          <button className="link" onClick={onStart}>
            Problems
          </button>{" "}
          tab.
        </p>
        {first?.friendly && (
          <p className="start-here">
            <b>Start with the first error:</b> <Inline text={first.friendly} />
          </p>
        )}
      </>
    );
  }
  if (kind !== "test") return <p className="muted">That was a plain run. Press <b>Run Tests</b> to grade it.</p>;
  if (result.tests.length === 0) return <p className="banner error">No test results were produced.{result.signal ? ` The program ended with ${result.signal}.` : ""}</p>;

  const pass = result.tests.filter((t) => t.status === "pass").length;
  return (
    <>
      {result.timedOut ? (
        <p className="banner error" role="status">
          The run was stopped by the time limit: {pass} of {result.tests.length} tests had passed when it ended, and the rest never ran. Look for a loop that does not
          end, or a thread that waits for ever (a deadlock, or a notification that never comes).
        </p>
      ) : (
        <p className={pass === result.tests.length ? "banner ok" : "banner"} role="status">
          {pass === result.tests.length ? "All tests passed 🎉" : `${pass} of ${result.tests.length} tests passed`}
        </p>
      )}
      {TIERS.filter((t) => declared.includes(t.id)).map((tier) => {
        const rows = result.tests.filter((t) => (t.tier ?? "works") === tier.id);
        const done = rows.length > 0 && rows.every((r) => r.status === "pass");
        return (
          <div key={tier.id} className="tier">
            <h4>
              {tier.label} {done ? <span className="badge ok">earned</span> : <span className="badge">{rows.filter((r) => r.status === "pass").length}/{rows.length}</span>}
            </h4>
            <ul className="tests">
              {rows.map((t, i) => (
                <li key={i} className={`t-${t.status}`}>
                  <span className="ico" aria-hidden="true">{STATUS[t.status].icon}</span>
                  <span className="sr">{STATUS[t.status].label}: </span>
                  <span className="tname">{t.name}</span>
                  {t.message && <pre className="tmsg">{t.message}</pre>}
                </li>
              ))}
            </ul>
          </div>
        );
      })}
    </>
  );
}

/** What a sanitizer found, in plain language: the headline, the explanation, where it happened and where the memory came from. */
function RuntimeCard({ report, raw, stopped }: { report: RuntimeReport; raw: string; stopped: number }) {
  const why = explainReport(report);
  const at = (f: ReportFrame) => `${f.file}:${f.line}`;
  return (
    <div className="runtime-card" role="alert">
      <p className="banner error">
        <b>{why?.title ?? report.summary}</b>
        {report.location && (
          <>
            {" "}at <code>{report.location.file}:{report.location.line}</code>
          </>
        )}
        {stopped > 1 && <span className="muted"> ({stopped} tests were stopped by a sanitizer; this is the first)</span>}
      </p>
      {why && (
        <p className="start-here">
          <Inline text={why.text} />
        </p>
      )}
      <LessonLink id={why?.lesson ?? undefined} />
      {(report.frames.length > 0 || report.allocatedAt || report.freedAt || report.previousAt) && (
        <ul className="frames">
          {report.frames.map((f, i) => (
            <li key={i}>
              {i === 0 ? "happened in" : "called from"} <code>{f.function || "(unknown)"}</code> at <code>{at(f)}</code>
            </li>
          ))}
          {report.freedAt && (
            <li>
              freed at <code>{at(report.freedAt)}</code>
            </li>
          )}
          {report.allocatedAt && (
            <li>
              allocated at <code>{at(report.allocatedAt)}</code>
            </li>
          )}
          {report.previousAt && (
            <li>
              {report.previousAccess ? `the other thread (${report.previousAccess.toLowerCase()})` : "the other thread"} at <code>{at(report.previousAt)}</code>
            </li>
          )}
        </ul>
      )}
      <details className="raw">
        <summary>Sanitizer report ({report.tool})</summary>
        <pre className="console err">{raw}</pre>
      </details>
    </div>
  );
}

function Console({ result }: { result: RunResult }) {
  if (!result.compiled) return <p className="muted">Nothing ran: compilation failed.</p>;
  return (
    <>
      <div className="meta">
        {result.timedOut ? <span className="badge bad">⏱ timed out (infinite loop?)</span> : result.signal ? <span className="badge bad">⚠ {result.signal}</span> : <span className="badge">exit code {result.exitCode}</span>}
      </div>
      <h4>stdout</h4>
      <pre className="console">{result.stdout || <span className="muted">(empty)</span>}</pre>
      {result.stderr && (
        <>
          <h4>stderr</h4>
          <pre className="console err">{result.stderr}</pre>
        </>
      )}
    </>
  );
}

/** Renders `backticked` spans as inline code (friendly messages are plain text, not full markdown). */
function Inline({ text }: { text: string }) {
  return (
    <>
      {text.split("`").map((part, i) => (i % 2 === 1 ? <code key={i}>{part}</code> : part))}
    </>
  );
}

function LessonLink({ id }: { id?: string }) {
  const lesson = id ? getLesson(id) : undefined;
  if (!lesson) return null;
  return (
    <Link className="learn-more" to={`/lesson/${lesson.meta.id}`}>
      Learn more: {lesson.meta.title}
    </Link>
  );
}

function Explained({ d }: { d: CompileDiagnostic }) {
  return (
    <>
      <span className="ptext">{d.friendly ? <Inline text={d.friendly} /> : d.message}</span>
      {d.friendly && (
        <details className="raw">
          <summary>Compiler message</summary>
          <code>{d.message}</code>
        </details>
      )}
    </>
  );
}

function Problems({ diags, onJump, onExplain, raw }: { diags: CompileDiagnostic[]; onJump(d: CompileDiagnostic): void; onExplain?(d: CompileDiagnostic): void; raw: string }) {
  const [showRaw, setShowRaw] = useState(false);
  // Notes explain the diagnostic before them ("candidate function not viable..."): nest them under it.
  const groups: { main: CompileDiagnostic; notes: CompileDiagnostic[] }[] = [];
  for (const d of diags) {
    if (d.severity === "note" && groups.length > 0) groups[groups.length - 1]!.notes.push(d);
    else if (d.severity !== "note") groups.push({ main: d, notes: [] });
  }
  if (groups.length === 0 && !raw.trim()) return <p className="muted">No compiler errors or warnings. ✔</p>;
  return (
    <>
      <ul className="problems">
        {groups.map(({ main: d, notes }, i) => (
          <li key={i}>
            <button className={`prob ${d.severity}`} onClick={() => onJump(d)}>
              <span className="ico" aria-hidden="true">{d.severity === "error" ? "✘" : "⚠"}</span>
              <span className="sr">{d.severity}: </span>
              <code className="loc">{d.line > 0 ? `${d.file}:${d.line}:${d.column}` : d.file}</code>
              <Explained d={d} />
            </button>
            <LessonLink id={d.friendlyLesson} />
            {onExplain && (
              <button className="link explain" onClick={() => onExplain(d)}>
                Explain this error (AI tutor)
              </button>
            )}
            {notes.length > 0 && (
              <ul className="notes">
                {notes.map((n, j) => (
                  <li key={j}>
                    <Explained d={n} />
                  </li>
                ))}
              </ul>
            )}
          </li>
        ))}
      </ul>
      <button className="link" onClick={() => setShowRaw((s) => !s)}>{showRaw ? "Hide" : "Show"} raw compiler output</button>
      {showRaw && <pre className="console">{raw}</pre>}
    </>
  );
}
