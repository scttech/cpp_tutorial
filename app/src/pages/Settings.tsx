import { useRef, useState } from "react";
import { CLEAR_RANGE } from "../warmup/weak";
import { EDITOR_FONT, TEXT_SCALE, resetPrefs, savePrefs, usePrefs } from "../prefs/prefs";
import type { KeyMode, LayoutPref, ThemePref } from "../prefs/prefs";
import { applyImport, exportProgress, parseImport, resetAll, setPace, setWeakClear, summarize, useAllProgress, useStorageKind } from "../state/store";
import type { ImportSummary } from "../state/store";
import type { Progress } from "../state/model";
import { MODELS, MAX_TOKENS_LIMIT, clearApiKey, clearLeaks, looksLikeKey, readLeaks, saveTutorSettings, useTutorSettings } from "../tutor/settings";

const STORAGE_TEXT = {
  indexeddb: "Saved in this browser (IndexedDB).",
  localstorage: "IndexedDB is unavailable here, so progress is saved with the simpler localStorage fallback.",
  memory: "Browser storage is unavailable (a private window?). Progress will be lost when you close this tab: export it to keep it.",
} as const;

function TutorSettingsSection() {
  const t = useTutorSettings();
  const [key, setKey] = useState("");
  const [show, setShow] = useState(false);
  const [leaks, setLeaks] = useState(() => readLeaks().length);
  const known = MODELS.some((m) => m.id === t.model);
  return (
    <section className="tutor-key">
      <h2>AI tutor (optional)</h2>
      <p className="muted">
        A tutor you can talk to about each lesson: it nudges, explains and discusses, and it never sees the reference solution or the hidden tests. It uses <b>your own</b> Anthropic API key,
        and your browser talks directly to api.anthropic.com. The key is stored only in this browser and is never included in exported progress.
      </p>
      <p className="banner">Use a dedicated key with a spend limit. Anyone with access to this browser profile can read it.</p>
      {t.apiKey ? (
        <p>
          Key saved: <code>{`${t.apiKey.slice(0, 7)}…${t.apiKey.slice(-4)}`}</code>{" "}
          <button className="btn" onClick={clearApiKey}>
            Remove key
          </button>
        </p>
      ) : (
        <div className="row">
          <input
            type={show ? "text" : "password"}
            value={key}
            onChange={(e) => setKey(e.target.value)}
            placeholder="sk-ant-…"
            aria-label="Anthropic API key"
            autoComplete="off"
            spellCheck={false}
          />
          <label className="small">
            <input type="checkbox" checked={show} onChange={(e) => setShow(e.target.checked)} /> show
          </label>
          <button
            className="btn primary"
            disabled={!key.trim()}
            onClick={() => {
              saveTutorSettings({ apiKey: key });
              setKey("");
            }}
          >
            Save key
          </button>
        </div>
      )}
      {key.trim() !== "" && !looksLikeKey(key.trim()) && <p className="muted small">That does not look like an Anthropic key (they start with sk-ant-). You can still save it.</p>}
      <label>
        Model
        <select value={known ? t.model : "other"} onChange={(e) => e.target.value !== "other" && saveTutorSettings({ model: e.target.value })}>
          {MODELS.map((m) => (
            <option key={m.id} value={m.id}>
              {m.label}
            </option>
          ))}
          {!known && <option value="other">{t.model}</option>}
        </select>
      </label>
      <label>
        Longest reply (tokens): {t.maxTokens}
        <input type="range" min={256} max={MAX_TOKENS_LIMIT} step={128} value={t.maxTokens} onChange={(e) => saveTutorSettings({ maxTokens: Number(e.target.value) })} />
      </label>
      <p className="muted small">
        Guardrail catches so far: {leaks} (replies that were too long or reproduced the solution and were replaced).{" "}
        {leaks > 0 && (
          <button
            className="link"
            onClick={() => {
              clearLeaks();
              setLeaks(0);
            }}
          >
            Clear
          </button>
        )}
      </p>
    </section>
  );
}

const THEME_CHOICES: { id: ThemePref; label: string; hint: string }[] = [
  { id: "dark", label: "Dark", hint: "The default." },
  { id: "light", label: "Light", hint: "Dark text on a light page." },
  { id: "contrast", label: "High contrast", hint: "Black and white with bright accents and strong borders." },
  { id: "system", label: "Match my system", hint: "Follows your operating system's light, dark and high-contrast settings." },
];

const KEY_CHOICES: { id: KeyMode; label: string; hint: string }[] = [
  { id: "default", label: "Standard", hint: "Ordinary editing keys." },
  { id: "vim", label: "Vim", hint: "Modal editing; the mode shows under the editor." },
  { id: "emacs", label: "Emacs", hint: "Ctrl+A, Ctrl+E, Ctrl+K and friends." },
];

const LAYOUT_CHOICES: { id: LayoutPref; label: string; hint: string }[] = [
  { id: "auto", label: "Automatic", hint: "Results beside the editor on a wide screen (1800px and up), stacked below it otherwise." },
  { id: "wide", label: "Always wide", hint: "Lesson, editor and results side by side." },
  { id: "normal", label: "Always stacked", hint: "Results below the editor." },
];

function AppearanceSection() {
  const p = usePrefs();
  return (
    <section aria-labelledby="appearance-h">
      <h2 id="appearance-h">Appearance</h2>
      <fieldset className="choice-set">
        <legend>Theme</legend>
        {THEME_CHOICES.map((t) => (
          <label key={t.id} className="radio">
            <input type="radio" name="theme" checked={p.theme === t.id} onChange={() => savePrefs({ theme: t.id })} /> {t.label} <span className="muted small">{t.hint}</span>
          </label>
        ))}
      </fieldset>
      <fieldset className="choice-set">
        <legend>Lesson page layout</legend>
        {LAYOUT_CHOICES.map((l) => (
          <label key={l.id} className="radio">
            <input type="radio" name="layout" checked={p.layout === l.id} onChange={() => savePrefs({ layout: l.id })} /> {l.label} <span className="muted small">{l.hint}</span>
          </label>
        ))}
      </fieldset>
      <label>
        Text size: {p.textScale}%
        <input type="range" min={TEXT_SCALE.min} max={TEXT_SCALE.max} step={TEXT_SCALE.step} value={p.textScale} onChange={(e) => savePrefs({ textScale: Number(e.target.value) })} />
      </label>
      <label className="radio">
        <input type="checkbox" checked={p.motion === "reduce"} onChange={(e) => savePrefs({ motion: e.target.checked ? "reduce" : "system" })} /> Reduce motion{" "}
        <span className="muted small">Turns off animations and transitions. Already on if your system asks for it.</span>
      </label>
    </section>
  );
}

function EditorSection() {
  const p = usePrefs();
  return (
    <section aria-labelledby="editor-h">
      <h2 id="editor-h">Code editor</h2>
      <label>
        Font size: {p.editorFont}px
        <input type="range" min={EDITOR_FONT.min} max={EDITOR_FONT.max} step={1} value={p.editorFont} onChange={(e) => savePrefs({ editorFont: Number(e.target.value) })} />
      </label>
      <label className="radio">
        <input type="checkbox" checked={p.wordWrap} onChange={(e) => savePrefs({ wordWrap: e.target.checked })} /> Wrap long lines
      </label>
      <fieldset className="choice-set">
        <legend>Keybindings</legend>
        {KEY_CHOICES.map((k) => (
          <label key={k.id} className="radio">
            <input type="radio" name="keys" checked={p.keys === k.id} onChange={() => savePrefs({ keys: k.id })} /> {k.label} <span className="muted small">{k.hint}</span>
          </label>
        ))}
      </fieldset>
      <p className="muted small">
        Keyboard users: Tab inserts a tab inside the editor. Press <kbd>Ctrl</kbd>+<kbd>M</kbd> to make Tab move focus out instead. Run is <kbd>Ctrl</kbd>+<kbd>Enter</kbd>, Run Tests is <kbd>Ctrl</kbd>+<kbd>Shift</kbd>+<kbd>Enter</kbd>.
      </p>
      <p className="row">
        <button className="btn" onClick={resetPrefs}>
          Restore display defaults
        </button>
      </p>
    </section>
  );
}

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

      <AppearanceSection />
      <EditorSection />

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
        <h2>Weak spots</h2>
        <p className="muted">A lesson or topic you keep missing shows on the home page until you answer this many of its warm-up cards right in a row.</p>
        <label>
          Clear a weak spot after {state.settings.weakClear} right answer{state.settings.weakClear === 1 ? "" : "s"} in a row
          <input type="range" min={CLEAR_RANGE.min} max={CLEAR_RANGE.max} step={1} value={state.settings.weakClear} onChange={(e) => setWeakClear(Number(e.target.value))} />
        </label>
      </section>

      <TutorSettingsSection />

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
