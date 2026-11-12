import Editor from "@monaco-editor/react";
import type { OnMount } from "@monaco-editor/react";
import type * as Monaco from "monaco-editor";
import { useEffect, useRef, useState } from "react";
import "../monaco";
import { activeTheme, usePrefs } from "../prefs/prefs";
import type { ResolvedTheme } from "../prefs/prefs";
import { Tabs, panelProps } from "./Tabs";
import type { CompileDiagnostic } from "../runner/types";

interface Props {
  files: Record<string, string>;
  active: string;
  readonlyFiles: string[];
  diagnostics: CompileDiagnostic[];
  onChange(file: string, value: string): void;
  onSelect(file: string): void;
  onRun(): void;
  onRunTests(): void;
  jumpTo?: { file: string; line: number; column: number; nonce: number } | null;
}

const SEVERITY = { error: 8, warning: 4, note: 2 } as const; // monaco MarkerSeverity values

const MONACO_THEME: Record<ResolvedTheme, string> = { dark: "vs-dark", light: "vs", contrast: "hc-black" };

export function CodeEditor(p: Props) {
  const [ready, setReady] = useState(false);
  const editorRef = useRef<Monaco.editor.IStandaloneCodeEditor | null>(null);
  const monacoRef = useRef<typeof Monaco | null>(null);
  // Keep latest callbacks reachable from Monaco commands registered once at mount.
  const cb = useRef({ run: p.onRun, tests: p.onRunTests });
  cb.current = { run: p.onRun, tests: p.onRunTests };
  const latest = useRef(p);
  latest.current = p;

  // Which file does a change belong to? Ask the editor's CURRENT model rather than trusting the
  // `active` prop captured by a render: when the user switches tabs Monaco swaps models and emits a
  // change event, and attributing that to the previously active file would overwrite it with the
  // new file's text. Events that do not actually change the file's text are ignored.
  const handleChange = (value: string | undefined) => {
    const name = editorRef.current?.getModel()?.uri.path.replace(/^\//, "");
    const cur = latest.current;
    if (!name || value === undefined || cur.files[name] === undefined) return;
    if (cur.files[name] === value) return;
    if (cur.readonlyFiles.includes(name)) return;
    cur.onChange(name, value);
  };

  const onMount: OnMount = (editor, monaco) => {
    editorRef.current = editor;
    monacoRef.current = monaco;
    setReady(true);
    editor.addCommand(monaco.KeyMod.CtrlCmd | monaco.KeyCode.Enter, () => cb.current.run());
    editor.addCommand(monaco.KeyMod.CtrlCmd | monaco.KeyMod.Shift | monaco.KeyCode.Enter, () => cb.current.tests());
  };

  // Compiler diagnostics -> squiggles on the matching file's model.
  useEffect(() => {
    const monaco = monacoRef.current;
    if (!monaco) return;
    for (const model of monaco.editor.getModels()) {
      const name = model.uri.path.replace(/^\//, "");
      const markers = p.diagnostics
        .filter((d) => d.file === name && d.line > 0 && d.severity !== "note")
        .map((d) => ({
          severity: SEVERITY[d.severity],
          message: d.friendly ? `${d.friendly}\n\n${d.message}` : d.message,
          startLineNumber: d.line,
          startColumn: d.column,
          endLineNumber: d.line,
          endColumn: Math.max(d.column + 1, model.getLineMaxColumn(Math.min(d.line, model.getLineCount()))),
        }));
      monaco.editor.setModelMarkers(model, "clang", markers);
    }
  }, [p.diagnostics, p.files, p.active]);

  useEffect(() => {
    if (p.jumpTo && p.jumpTo.file === p.active && editorRef.current) {
      editorRef.current.revealLineInCenter(p.jumpTo.line);
      editorRef.current.setPosition({ lineNumber: p.jumpTo.line, column: p.jumpTo.column });
      editorRef.current.focus();
    }
  }, [p.jumpTo, p.active]);

  // Vim or Emacs keys: loaded on demand so the default editor does not pay for them.
  const prefs = usePrefs();
  const statusRef = useRef<HTMLDivElement>(null);
  useEffect(() => {
    const editor = editorRef.current;
    if (!ready || !editor || prefs.keys === "default") return;
    let off: { dispose(): void } | null = null;
    let cancelled = false;
    void (async () => {
      try {
        if (prefs.keys === "vim") {
          const { initVimMode } = await import("monaco-vim");
          if (!cancelled && statusRef.current) off = initVimMode(editor, statusRef.current);
        } else {
          const { EmacsExtension } = await import("monaco-emacs");
          if (cancelled) return;
          const emacs = new EmacsExtension(editor);
          emacs.start();
          off = emacs;
        }
      } catch (e) {
        console.error("Could not load the keybindings; using the default ones.", e);
      }
    })();
    return () => {
      cancelled = true;
      off?.dispose();
    };
  }, [ready, prefs.keys]);

  const names = Object.keys(p.files);
  const label = `Code editor, ${p.active}${p.readonlyFiles.includes(p.active) ? ", read-only" : ""}. Press Ctrl+M to make the Tab key move focus out of the editor.`;
  return (
    <div className="editor">
      <Tabs
        prefix="files"
        label="Source files"
        tabs={names.map((n) => ({ id: n, label: <>{n}{p.readonlyFiles.includes(n) && <span className="ro"> (read-only)</span>}</> }))}
        value={p.active}
        onChange={p.onSelect}
      />
      <Editor
        height="100%"
        theme={MONACO_THEME[activeTheme()]}
        language="cpp"
        path={p.active}
        value={p.files[p.active] ?? ""}
        onChange={handleChange}
        onMount={onMount}
        wrapperProps={panelProps("files", p.active)}
        options={{
          readOnly: p.readonlyFiles.includes(p.active),
          minimap: { enabled: false },
          fontSize: prefs.editorFont,
          wordWrap: prefs.wordWrap ? "on" : "off",
          scrollBeyondLastLine: false,
          automaticLayout: true,
          tabSize: 4,
          renderWhitespace: "selection",
          ariaLabel: label,
          accessibilitySupport: "auto",
        }}
      />
      {prefs.keys === "vim" && <div ref={statusRef} className="vim-status" aria-live="polite" />}
    </div>
  );
}
