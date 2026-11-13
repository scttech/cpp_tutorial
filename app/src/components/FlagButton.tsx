import { useEffect, useRef, useState } from "react";
import type { Flag, NewFlag } from "../state/flags";
import { addFlagNote, useAllProgress } from "../state/store";

interface Props {
  target: Flag["target"];
  /** What to attach besides the note: called when the flag is saved (lessons add the code and the last run). */
  collect?: () => Pick<NewFlag, "code" | "result" | "context">;
  label?: string;
  /** Which edge of the button the form lines up with (use "left" where there is no room on the left of it). */
  align?: "left" | "right";
}

/** A button that opens a small form for a note about this lesson or card, saved with enough to reproduce it. Esc closes it. */
export function FlagButton({ target, collect, label = "Flag", align = "right" }: Props) {
  const [open, setOpen] = useState(false);
  const [note, setNote] = useState("");
  const [attach, setAttach] = useState(true);
  const [saved, setSaved] = useState<number | null>(null);
  const area = useRef<HTMLTextAreaElement>(null);
  const count = useAllProgress().flags.length;

  useEffect(() => {
    if (open) area.current?.focus();
  }, [open]);
  useEffect(() => {
    if (saved === null) return;
    const t = setTimeout(() => setSaved(null), 5000);
    return () => clearTimeout(t);
  }, [saved]);

  const save = () => {
    const extra = collect ? collect() : {};
    addFlagNote({ target, note, context: extra.context, ...(attach ? { code: extra.code, result: extra.result } : {}) });
    setSaved(count + 1);
    setNote("");
    setOpen(false);
  };

  return (
    <span className="flag-wrap">
      <button className="btn" type="button" aria-expanded={open} aria-haspopup="dialog" onClick={() => setOpen((o) => !o)}>
        {label}
      </button>
      <span className="sr" role="status">
        {saved !== null ? `Flagged. ${saved} saved.` : ""}
      </span>
      {saved !== null && (
        <span className="muted small flag-saved" aria-hidden="true">
          Flagged ({saved} saved)
        </span>
      )}
      {open && (
        <form
          className={`flag-panel align-${align}`}
          role="dialog"
          aria-label={`Flag this ${target.type === "lesson" ? "lesson" : "card"}`}
          onSubmit={(e) => {
            e.preventDefault();
            save();
          }}
          onKeyDown={(e) => {
            if (e.key === "Escape") {
              e.stopPropagation();
              setOpen(false);
            }
          }}
        >
          <label>
            What is wrong or confusing?
            <textarea ref={area} rows={4} value={note} onChange={(e) => setNote(e.target.value)} placeholder="A test that seems wrong, an unclear explanation, a typo…" />
          </label>
          {target.type === "lesson" && (
            <label className="radio small">
              <input type="checkbox" checked={attach} onChange={(e) => setAttach(e.target.checked)} /> Include my code and the last result
            </label>
          )}
          <div className="row">
            <button className="btn primary" type="submit" disabled={note.trim() === ""}>
              Save flag
            </button>
            <button className="btn" type="button" onClick={() => setOpen(false)}>
              Cancel
            </button>
          </div>
        </form>
      )}
    </span>
  );
}
