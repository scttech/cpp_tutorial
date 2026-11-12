import { useRef, useState } from "react";
import { Markdown } from "./Markdown";
import type { Demo, Gotcha } from "../content/gotchas";
import { runner } from "../runner";
import type { RunResult } from "../runner";

/** `backticked` spans as inline code (titles are one plain line, not full markdown). */
function Ticks({ text }: { text: string }) {
  return (
    <>
      {text.split("`").map((part, i) => (i % 2 === 1 ? <code key={i}>{part}</code> : part))}
    </>
  );
}

const FLAGS = ["-std=c++20", "-Wall", "-Wextra"];
const SANITIZE = ["-fsanitize=address,undefined"];
const THREAD = ["-pthread", "-fsanitize=thread"];

/** One editable, runnable program of a demo. Nothing here is graded and nothing is recorded. */
function Snip({ label, tone, initial, sanitize, server, thread }: { label: string; tone: "bad" | "fix"; initial: string; sanitize: boolean; server: boolean; thread: boolean }) {
  const [code, setCode] = useState(initial);
  const [result, setResult] = useState<RunResult | null>(null);
  const [error, setError] = useState<string | null>(null);
  const [busy, setBusy] = useState(false);
  const busyRef = useRef(false);
  // sanitizers exist only on the server runner; `server` marks demos that need anything else the browser compiler lacks
  const unavailable = ((sanitize || thread) && !runner.capabilities.has("sanitizers")) || ((server || thread) && runner.id !== "server");

  const run = async () => {
    if (busyRef.current) return;
    busyRef.current = true;
    setBusy(true);
    setError(null);
    try {
      const res = await runner.run({ mode: "run", userFiles: { "main.cpp": code }, testFiles: {}, flags: thread ? [...FLAGS, ...THREAD] : sanitize ? [...FLAGS, ...SANITIZE] : FLAGS, limits: { wallMs: 8000 } });
      setResult(res);
    } catch (e) {
      setResult(null);
      setError(e instanceof Error ? e.message : String(e));
    } finally {
      busyRef.current = false;
      setBusy(false);
    }
  };

  return (
    <div className={`snip ${tone}`}>
      <div className="snip-head">
        <b>{label}</b>
        <span className="spacer" />
        {code !== initial && (
          <button className="link" onClick={() => setCode(initial)}>
            Reset
          </button>
        )}
        <button className="btn" onClick={() => void run()} disabled={busy || unavailable} title={unavailable ? "This one needs the full (Docker) version" : "Compile and run"}>
          {busy ? "Running…" : "▶ Run"}
        </button>
      </div>
      <textarea value={code} onChange={(e) => setCode(e.target.value)} spellCheck={false} wrap="off" rows={Math.min(18, code.split("\n").length + 1)} aria-label={`${label}: code`} />
      {unavailable && <p className="muted small">Needs the full (Docker) version{sanitize || thread ? ": it uses the sanitizers" : ""}.</p>}
      {error && <p className="banner error">{error}</p>}
      {result && <Output r={result} />}
    </div>
  );
}

/** A sanitizer report is long: the first lines say what and where, the rest is a stack trace. */
const head = (text: string, lines = 8) => {
  const all = text.trim().split("\n");
  return all.length > lines ? [...all.slice(0, lines), "..."].join("\n") : all.join("\n");
};

function Output({ r }: { r: RunResult }) {
  if (!r.compiled) return <pre className="console err snip-out">{r.compileOutput.trim() || "It did not compile."}</pre>;
  const warnings = r.compileOutput.trim();
  return (
    <>
      {warnings && <pre className="console snip-out warn">{warnings}</pre>}
      {r.stdout !== "" && <pre className="console snip-out">{r.stdout}</pre>}
      {r.stderr.trim() !== "" && <pre className="console err snip-out">{head(r.stderr)}</pre>}
      <p className="muted small">{r.timedOut ? "Stopped by the time limit." : r.signal ? `Ended by signal ${r.signal}.` : `Exit code ${r.exitCode ?? "?"}.`}</p>
    </>
  );
}

function DemoBlock({ demo }: { demo: Demo }) {
  return (
    <details className="demo">
      <summary>Try it: the trap and the fix</summary>
      <div className="snips">
        <Snip label="The trap" tone="bad" initial={demo.bad.code} sanitize={demo.sanitize} server={demo.server} thread={demo.thread} />
        <Snip label="The fix" tone="fix" initial={demo.fix.code} sanitize={demo.sanitize} server={demo.server} thread={demo.thread} />
      </div>
    </details>
  );
}

// The C++ traps this lesson teaches about (spec 9.1): a short explanation for each tag in lesson.yaml.
export function GotchaList({ gotchas }: { gotchas: Gotcha[] }) {
  return (
    <div>
      <p className="muted">Watch out for these. Each one is a mistake that compiles, or that looks right, and the lesson&rsquo;s tests are built to catch it.</p>
      <ul className="gotchas">
        {gotchas.map((g) => (
          <li key={g.id}>
            <span className="g-title">
              <Ticks text={g.title} />
            </span>
            {g.text && (
              <div className="g-text">
                <Markdown>{g.text}</Markdown>
              </div>
            )}
            {g.demo && <DemoBlock demo={g.demo} />}
          </li>
        ))}
      </ul>
    </div>
  );
}
