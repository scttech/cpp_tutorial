import { useEffect, useMemo, useRef, useState } from "react";
import { Link } from "react-router-dom";
import { Markdown } from "./Markdown";
import type { Lesson } from "../content/types";
import type { RunResult } from "../runner/types";
import { recordAiHelp } from "../state/store";
import type { LessonProgress } from "../state/model";
import { buildReference } from "../tutor/guardrail";
import type { ReferenceSet } from "../tutor/guardrail";
import { MODES, modeInfo } from "../tutor/modes";
import type { TutorMode } from "../tutor/modes";
import type { ChatMessage } from "../tutor/api";
import { ask } from "../tutor/session";
import { logLeak, useTutorSettings } from "../tutor/settings";

export interface TutorRequest {
  nonce: number;
  mode: TutorMode;
  text: string;
}

interface Props {
  lesson: Lesson;
  files: Record<string, string>;
  result: RunResult | null;
  progress: LessonProgress;
  /** Set by the "Explain this error" button elsewhere on the page. */
  request: TutorRequest | null;
}

interface Turn {
  role: "user" | "assistant";
  text: string;
  mode: TutorMode;
  fellBack?: boolean;
}

function diagnosticLines(result: RunResult | null): string[] {
  if (!result) return [];
  return result.diagnostics.filter((d) => d.severity !== "note").map((d) => `${d.file}:${d.line}:${d.column}: ${d.severity}: ${d.message}`);
}

// The learner's own tutor: it sees the lesson, their code and their failures, and never the solution.
export function TutorPanel({ lesson, files, result, progress, request }: Props) {
  const settings = useTutorSettings();
  const [mode, setMode] = useState<TutorMode>("nudge");
  const [draft, setDraft] = useState("");
  const [turns, setTurns] = useState<Turn[]>([]);
  const [live, setLive] = useState("");
  const [busy, setBusy] = useState(false);
  const [error, setError] = useState<string | null>(null);
  const [tokens, setTokens] = useState({ input: 0, output: 0 });
  const abort = useRef<AbortController | null>(null);
  const reference = useRef<ReferenceSet | null>(null);
  const handled = useRef(0);
  const endRef = useRef<HTMLDivElement>(null);
  const latest = useRef({ files, result, progress, turns });
  latest.current = { files, result, progress, turns };
  const complete = progress.status === "complete";
  const info = modeInfo(mode);

  useEffect(
    () => () => {
      abort.current?.abort();
    },
    [],
  );
  useEffect(() => {
    endRef.current?.scrollIntoView?.({ block: "end" });
  }, [turns, live, busy]);

  const send = async (m: TutorMode, text: string) => {
    if (abort.current || !settings.apiKey) return;
    const mi = modeInfo(m);
    const question = text.trim() || mi.defaultQuestion || "Can you help?";
    const { files: code, result: res, progress: prog, turns: past } = latest.current;
    const ctl = new AbortController();
    abort.current = ctl;
    setBusy(true);
    setError(null);
    setLive("");
    setTurns((t) => [...t, { role: "user", text: question, mode: m }]);
    try {
      if (mi.guarded && reference.current === null) reference.current = buildReference(await lesson.loadSolution(), lesson.starter);
      const history: ChatMessage[] = past.map((t) => ({ role: t.role, content: t.text }));
      const out = await ask({
        apiKey: settings.apiKey,
        model: settings.model,
        maxTokens: settings.maxTokens,
        mode: m,
        question,
        history,
        context: {
          title: lesson.meta.title,
          lessonText: lesson.lessonMd,
          code,
          diagnostics: diagnosticLines(res),
          failingTests: (res?.tests ?? []).filter((t) => t.status !== "pass").map((t) => ({ name: t.name, message: t.message })),
          hintsUsed: prog.hintsUsed,
          aiHelpUsed: prog.aiHelpUsed,
          complete: prog.status === "complete",
        },
        reference: reference.current,
        signal: ctl.signal,
        onText: (d) => setLive((s) => s + d),
        onLeak: (v) => logLeak({ at: new Date().toISOString(), lesson: lesson.meta.id, mode: m, reason: v.reason, detail: v.detail }),
      });
      setTurns((t) => [...t, { role: "assistant", text: out.text, mode: m, fellBack: out.fellBack }]);
      setTokens((k) => ({ input: k.input + out.usage.input, output: k.output + out.usage.output }));
      recordAiHelp(lesson.meta.id, mi.explains);
      setDraft("");
    } catch (e) {
      // the question stays unanswered: take it back out of the conversation so the next turn is not confused
      setTurns((t) => t.slice(0, -1));
      if (e instanceof DOMException && e.name === "AbortError") setError("Cancelled.");
      else setError(e instanceof Error ? e.message : String(e));
      setDraft(question);
    } finally {
      abort.current = null;
      setBusy(false);
      setLive("");
    }
  };

  // "Explain this error" elsewhere on the page asks through here
  useEffect(() => {
    if (!request || request.nonce === handled.current) return;
    handled.current = request.nonce;
    setMode(request.mode);
    void send(request.mode, request.text);
    // eslint-disable-next-line react-hooks/exhaustive-deps
  }, [request]);

  const modes = useMemo(() => MODES, []);

  if (!settings.apiKey) {
    return (
      <div className="tutor">
        <h3>AI tutor</h3>
        <p>
          A tutor you can talk to about this lesson. It nudges, explains and discusses, and it is built so that it <b>never sees the solution</b>.
        </p>
        <p>
          It uses your own Anthropic API key, kept only in this browser. Add one in <Link to="/settings">Settings</Link>.
        </p>
      </div>
    );
  }

  return (
    <div className="tutor">
      <div className="tutor-turns" aria-live="polite">
        {turns.length === 0 && !busy && (
          <p className="muted">
            Pick a mode and ask. <b>Discuss</b> is for talking an idea through; <b>Nudge</b> and the others help when you are stuck. The tutor sees your code and your latest test results, not the solution.
          </p>
        )}
        {turns.map((t, i) =>
          t.role === "user" ? (
            <p key={i} className="turn user">
              <span className="who">You · {modeInfo(t.mode).label}</span>
              {t.text}
            </p>
          ) : (
            <div key={i} className={t.fellBack ? "turn tutor-reply fell-back" : "turn tutor-reply"}>
              <span className="who">Tutor</span>
              <Markdown>{t.text}</Markdown>
            </div>
          ),
        )}
        {busy && (
          <div className="turn tutor-pending">
            <span className="who">Tutor</span>
            {live ? <Markdown>{live}</Markdown> : <span className="muted">Thinking…</span>}
          </div>
        )}
        {error && (
          <p className="banner error" role="alert">
            {error}
          </p>
        )}
        <div ref={endRef} />
      </div>
      <form
        className="tutor-form"
        onSubmit={(e) => {
          e.preventDefault();
          void send(mode, draft);
        }}
      >
        <label>
          <span className="sr">Help mode</span>
          <select value={mode} onChange={(e) => setMode(e.target.value as TutorMode)} disabled={busy}>
            {modes.map((m) => (
              <option key={m.id} value={m.id} disabled={m.needsComplete && !complete}>
                {m.label}
                {m.needsComplete && !complete ? " (pass the tests first)" : ""}
              </option>
            ))}
          </select>
        </label>
        <p className="muted small">{info.blurb}</p>
        <textarea
          value={draft}
          rows={3}
          disabled={busy}
          placeholder={info.defaultQuestion ? `Optional: ${info.defaultQuestion}` : "What would you like to ask?"}
          aria-label="Your question for the tutor"
          onChange={(e) => setDraft(e.target.value)}
          onKeyDown={(e) => {
            if (e.key === "Enter" && (e.ctrlKey || e.metaKey)) {
              e.preventDefault();
              void send(mode, draft);
            }
          }}
        />
        <div className="row">
          <button className="btn primary" type="submit" disabled={busy || (!draft.trim() && !info.defaultQuestion)}>
            Ask
          </button>
          {busy && (
            <button className="btn" type="button" onClick={() => abort.current?.abort()}>
              Cancel
            </button>
          )}
          {complete && (
            <button
              className="btn"
              type="button"
              disabled={busy}
              onClick={() => {
                setMode("practice");
                void send("practice", "");
              }}
            >
              Another one like this
            </button>
          )}
          {turns.length > 0 && !busy && (
            <button className="btn" type="button" onClick={() => setTurns([])}>
              Clear chat
            </button>
          )}
          <span className="spacer" />
          <span className="muted small" title="Tokens used by the tutor since you opened this lesson">
            {tokens.input + tokens.output > 0 ? `${tokens.input.toLocaleString()} in · ${tokens.output.toLocaleString()} out` : "Each question counts as AI help"} · {progress.aiHelpUsed} used
          </span>
        </div>
      </form>
    </div>
  );
}
