import { DiffEditor } from "@monaco-editor/react";
import * as monaco from "monaco-editor";
import { useEffect, useMemo, useState } from "react";
import "../monaco";
import { activeTheme, usePrefs } from "../prefs/prefs";
import { MONACO_THEME } from "./CodeEditor";
import { Tabs, panelProps } from "./Tabs";

interface Props {
  /** The learner's files and the reference solution's files. */
  mine: Record<string, string>;
  reference: Record<string, string>;
  onClose(): void;
  /** Present when the AI tutor is set up. */
  onReview?(): void;
}

/** How many comparisons are mounted right now. */
let live = 0;

/** The learner's passing solution next to the reference solution, file by file (read-only). */
export function CompareView({ mine, reference, onClose, onReview }: Props) {
  const prefs = usePrefs();
  const names = useMemo(() => [...new Set([...Object.keys(reference), ...Object.keys(mine).filter((n) => n in reference)])], [reference, mine]);
  const [active, setActive] = useState(names[0] ?? "main.cpp");
  const [inline, setInline] = useState(false);
  // The diff widget cannot dispose its own models without an error ("TextModel got disposed before DiffEditorWidget model got reset"),
  // so it keeps them and they are disposed here, once it has gone.
  useEffect(() => {
    live += 1;
    return () => {
      live -= 1;
      // Deferred, and only when no comparison is showing: React's development double-mount unmounts and remounts at once.
      setTimeout(() => {
        if (live > 0) return;
        for (const m of monaco.editor.getModels()) if (m.uri.path.startsWith("/reference/") || m.uri.path.startsWith("/mine/")) m.dispose();
      }, 0);
    };
  }, []);
  const same = (mine[active] ?? "").trim() === (reference[active] ?? "").trim();
  const differing = names.filter((n) => (mine[n] ?? "").trim() !== (reference[n] ?? "").trim()).length;

  return (
    <div className="compare" role="region" aria-label="Compare with the reference solution">
      <div className="compare-bar">
        <span>
          <b>Reference</b> on the left, <b>your solution</b> on the right.{" "}
          <span className="muted small">{differing === 0 ? "Every file matches the reference." : `${differing} of ${names.length} file${names.length === 1 ? "" : "s"} differ.`}</span>
        </span>
        <span className="spacer" />
        <label className="small">
          <input type="checkbox" checked={inline} onChange={(e) => setInline(e.target.checked)} /> Inline
        </label>
        {onReview && (
          <button className="btn" type="button" onClick={onReview}>
            Ask the tutor to review mine
          </button>
        )}
        <button className="btn" type="button" onClick={onClose}>
          Back to the editor
        </button>
      </div>
      <Tabs prefix="cmp" label="Files to compare" tabs={names.map((n) => ({ id: n, label: n }))} value={active} onChange={setActive} />
      {same && (
        <p className="banner ok" role="status">
          {active} is the same as the reference (ignoring blank space at the ends).
        </p>
      )}
      <DiffEditor
        height="100%"
        theme={MONACO_THEME[activeTheme()]}
        language="cpp"
        original={reference[active] ?? ""}
        modified={mine[active] ?? ""}
        originalModelPath={`reference/${active}`}
        modifiedModelPath={`mine/${active}`}
        keepCurrentOriginalModel
        keepCurrentModifiedModel
        wrapperProps={panelProps("cmp", active)}
        options={{
          readOnly: true,
          originalEditable: false,
          renderSideBySide: !inline,
          minimap: { enabled: false },
          fontSize: prefs.editorFont,
          wordWrap: prefs.wordWrap ? "on" : "off",
          scrollBeyondLastLine: false,
          automaticLayout: true,
          ariaLabel: `Differences between the reference and your ${active}`,
        }}
      />
    </div>
  );
}
