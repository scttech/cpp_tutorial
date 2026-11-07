import { useState } from "react";
import type { CompileDiagnostic, RunResult, Tier, TestResult } from "../runner/types";

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

interface Props {
  result: RunResult | null;
  kind: "run" | "test" | null;
  busy: boolean;
  error: string | null;
  declaredTiers: Tier[];
  onJump(d: CompileDiagnostic): void;
}

export function ResultsPanel({ result, kind, busy, error, declaredTiers, onJump }: Props) {
  const [tab, setTab] = useState<"tests" | "console" | "problems">("tests");
  const problems = result?.diagnostics.filter((d) => d.severity !== "note") ?? [];
  const nErr = problems.filter((d) => d.severity === "error").length;

  return (
    <section className="results" aria-label="Results">
      <div className="tabs" role="tablist">
        {(["tests", "console", "problems"] as const).map((t) => (
          <button key={t} role="tab" aria-selected={tab === t} className={tab === t ? "tab on" : "tab"} onClick={() => setTab(t)}>
            {t === "tests" ? "Tests" : t === "console" ? "Console" : `Problems${problems.length ? ` (${problems.length})` : ""}`}
          </button>
        ))}
        {busy && <span className="spinner" role="status">Running…</span>}
        {result && !busy && <span className="timing">compile {result.timings.compileMs} ms · run {result.timings.runMs} ms</span>}
      </div>
      <div className="panel-body">
        {error && <p className="banner error" role="alert">{error}</p>}
        {!result && !error && <p className="muted">Press <b>Run Tests</b> (Ctrl+Shift+Enter) to check your solution, or <b>Run</b> (Ctrl+Enter) to just run it.</p>}
        {result && tab === "tests" && <Tests result={result} kind={kind} declared={declaredTiers} nErr={nErr} />}
        {result && tab === "console" && <Console result={result} />}
        {result && tab === "problems" && <Problems list={problems} onJump={onJump} raw={result.compileOutput} />}
      </div>
    </section>
  );
}

function Tests({ result, kind, declared, nErr }: { result: RunResult; kind: "run" | "test" | null; declared: Tier[]; nErr: number }) {
  if (!result.compiled) {
    return <p className="banner error">Your code did not compile ({nErr} error{nErr === 1 ? "" : "s"}). See the <b>Problems</b> tab.</p>;
  }
  if (kind !== "test") return <p className="muted">That was a plain run. Press <b>Run Tests</b> to grade it.</p>;
  if (result.tests.length === 0) return <p className="banner error">No test results were produced.{result.signal ? ` The program ended with ${result.signal}.` : ""}</p>;

  const pass = result.tests.filter((t) => t.status === "pass").length;
  return (
    <>
      <p className={pass === result.tests.length ? "banner ok" : "banner"} role="status">
        {pass === result.tests.length ? "All tests passed 🎉" : `${pass} of ${result.tests.length} tests passed`}
      </p>
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

function Problems({ list, onJump, raw }: { list: CompileDiagnostic[]; onJump(d: CompileDiagnostic): void; raw: string }) {
  const [showRaw, setShowRaw] = useState(false);
  if (list.length === 0 && !raw.trim()) return <p className="muted">No compiler errors or warnings. ✔</p>;
  return (
    <>
      <ul className="problems">
        {list.map((d, i) => (
          <li key={i}>
            <button className={`prob ${d.severity}`} onClick={() => onJump(d)}>
              <span className="ico" aria-hidden="true">{d.severity === "error" ? "✘" : "⚠"}</span>
              <span className="sr">{d.severity}: </span>
              <code>{d.file}:{d.line}:{d.column}</code> {d.friendly ?? d.message}
            </button>
          </li>
        ))}
      </ul>
      <button className="link" onClick={() => setShowRaw((s) => !s)}>{showRaw ? "Hide" : "Show"} raw compiler output</button>
      {showRaw && <pre className="console">{raw}</pre>}
    </>
  );
}
