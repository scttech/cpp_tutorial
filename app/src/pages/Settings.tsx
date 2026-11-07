import { useRef, useState } from "react";
import { applyImport, exportProgress, parseImport, resetAll, setPace, summarize, useAllProgress, useStorageKind } from "../state/store";
import type { ImportSummary } from "../state/store";
import type { Progress } from "../state/model";

const STORAGE_TEXT = {
  indexeddb: "Saved in this browser (IndexedDB).",
  localstorage: "IndexedDB is unavailable here, so progress is saved with the simpler localStorage fallback.",
  memory: "Browser storage is unavailable (a private window?). Progress will be lost when you close this tab: export it to keep it.",
} as const;

export function Settings() {
  const state = useAllProgress();
  const storage = useStorageKind();
  const fileRef = useRef<HTMLInputElement>(null);
  const [pending, setPending] = useState<{ progress: Progress; summary: ImportSummary; name: string } | null>(null);
  const [message, setMessage] = useState<{ kind: "ok" | "error"; text: string } | null>(null);
  const mine = summarize(state);

  const download = () => {
    const blob = new Blob([exportProgress()], { type: "application/json" });
    const url = URL.createObjectURL(blob);
    const a = document.createElement("a");
    a.href = url;
    a.download = `zth-progress-${new Date().toISOString().slice(0, 10)}.json`;
    a.click();
    URL.revokeObjectURL(url);
    setMessage({ kind: "ok", text: "Progress exported." });
  };

  const onFile = async (file: File | undefined) => {
    setMessage(null);
    setPending(null);
    if (!file) return;
    if (file.size > 5 * 1024 * 1024) return setMessage({ kind: "error", text: "That file is too large to be a progress export." });
    const parsed = parseImport(await file.text());
    if (!parsed.ok) return setMessage({ kind: "error", text: parsed.error });
    setPending({ progress: parsed.progress, summary: parsed.summary, name: file.name });
  };

  const confirmImport = () => {
    if (!pending) return;
    applyImport(pending.progress);
    setMessage({ kind: "ok", text: `Imported ${pending.summary.complete} completed lessons and ${pending.summary.badges} badges.` });
    setPending(null);
    if (fileRef.current) fileRef.current.value = "";
  };

  const reset = async () => {
    if (!confirm("Erase ALL progress, badges and saved code? This cannot be undone. (Export first if you might want it back.)")) return;
    await resetAll();
    setMessage({ kind: "ok", text: "All progress erased." });
  };

  return (
    <main className="page settings">
      <h1>Settings</h1>

      <section>
        <h2>Pace</h2>
        <p className="muted">Experienced with other languages? Fast pace opens each lesson on its one-screen &ldquo;what&rsquo;s different in C++&rdquo; summary.</p>
        <label className="radio">
          <input type="radio" name="pace" checked={state.settings.pace === "full"} onChange={() => setPace("full")} /> Full: the complete lesson first
        </label>
        <label className="radio">
          <input type="radio" name="pace" checked={state.settings.pace === "fast"} onChange={() => setPace("fast")} /> Fast: the short summary first
        </label>
      </section>

      <section>
        <h2>Your data</h2>
        <p>
          {mine.complete} lessons complete · {mine.badges} badges · {mine.lessons} lessons started
        </p>
        <p className="muted small">{STORAGE_TEXT[storage]}</p>
        <div className="row">
          <button className="btn" onClick={download}>
            Export progress (.json)
          </button>
          <label className="btn">
            Import progress…
            <input ref={fileRef} type="file" accept="application/json,.json" hidden onChange={(e) => void onFile(e.target.files?.[0])} />
          </label>
        </div>
        {pending && (
          <div className="banner" role="alert">
            <p>
              <b>{pending.name}</b> contains {pending.summary.complete} completed lessons, {pending.summary.lessons} lessons started and {pending.summary.badges} badges.
              Importing <b>replaces</b> your current progress ({mine.complete} completed, {mine.badges} badges).
            </p>
            <div className="row">
              <button className="btn primary" onClick={confirmImport}>
                Replace my progress
              </button>
              <button className="btn" onClick={() => setPending(null)}>
                Cancel
              </button>
            </div>
          </div>
        )}
        {message && (
          <p className={`banner ${message.kind === "ok" ? "ok" : "error"}`} role="status">
            {message.text}
          </p>
        )}
      </section>

      <section>
        <h2>Danger zone</h2>
        <button className="btn danger" onClick={() => void reset()}>
          Erase all progress…
        </button>
      </section>
    </main>
  );
}
