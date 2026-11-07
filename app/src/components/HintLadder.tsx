import type { HintLevel } from "../content/types";

const NAMES = ["", "Nudge", "Concept", "Pinpoint", "Approach"];

interface Props {
  hints: HintLevel[];
  revealed: number; // how many levels are unlocked
  onReveal(): void;
}

// Hints unlock one level at a time and every reveal is counted (spec 9.2).
export function HintLadder({ hints, revealed, onReveal }: Props) {
  return (
    <div className="hints">
      <p className="muted">Hints unlock one step at a time. Using them is fine; it is recorded only so you can see your own progress.</p>
      <ol>
        {hints.map((h, i) => (
          <li key={h.level} className={i < revealed ? "open" : "locked"}>
            <b>Level {h.level}: {NAMES[h.level] ?? ""}</b>
            {i < revealed ? <div className="md-hint">{h.text}</div> : <span className="muted"> 🔒 locked</span>}
          </li>
        ))}
      </ol>
      {revealed < hints.length && <button className="btn" onClick={onReveal}>Reveal level {revealed + 1}</button>}
    </div>
  );
}
