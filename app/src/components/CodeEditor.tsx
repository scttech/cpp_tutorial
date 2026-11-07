import Editor from "@monaco-editor/react";
import type { OnMount } from "@monaco-editor/react";
import type * as Monaco from "monaco-editor";
import { useEffect, useRef } from "react";
import "../monaco";
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

export function CodeEditor(p: Props) {
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
        .filter((d) => d.file === name)
        .map((d) => ({
          severity: SEVERITY[d.severity],
          message: d.friendly ?? d.message,
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

  const names = Object.keys(p.files);
  return (
    <div className="editor">
      <div className="tabs" role="tablist" aria-label="Source files">
        {names.map((n) => (
          <button key={n} role="tab" aria-selected={n === p.active} className={n === p.active ? "tab on" : "tab"} onClick={() => p.onSelect(n)}>
            {n}
            {p.readonlyFiles.includes(n) && <span className="ro"> (read-only)</span>}
          </button>
        ))}
      </div>
      <Editor
        height="100%"
        theme="vs-dark"
        language="cpp"
        path={p.active}
        value={p.files[p.active] ?? ""}
        onChange={handleChange}
        onMount={onMount}
        options={{
          readOnly: p.readonlyFiles.includes(p.active),
          minimap: { enabled: false },
          fontSize: 14,
          scrollBeyondLastLine: false,
          automaticLayout: true,
          tabSize: 4,
          renderWhitespace: "selection",
        }}
      />
    </div>
  );
}
