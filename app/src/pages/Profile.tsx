import { useState } from "react";
import { Link } from "react-router-dom";
import { badgeDefs, catalog } from "../badges/catalog";
import { evaluate, isAvailable } from "../badges/engine";
import type { BadgeDef } from "../badges/engine";
import { getLesson } from "../content/loader";
import type { Progress } from "../state/model";
import { useAllProgress } from "../state/store";

const CATEGORIES: { id: BadgeDef["category"]; title: string }[] = [
  { id: "progress", title: "Progress" },
  { id: "mastery", title: "Mastery" },
  { id: "habits", title: "Learning habits" },
  { id: "consistency", title: "Consistency" },
  { id: "domain", title: "Domain tracks" },
  { id: "fun", title: "Fun & hidden" },
];

function Medallion({ def, earned }: { def: BadgeDef; earned: boolean }) {
  const hiddenLocked = def.hidden && !earned;
  return (
    <span className={`medal cat-${def.category} ${def.tier ? `tier-${def.tier}` : ""} ${earned ? "earned" : "locked"}`} aria-hidden="true">
      {hiddenLocked ? "?" : def.icon}
    </span>
  );
}

function BadgeCard({ def, state, selected, onSelect }: { def: BadgeDef; state: Progress; selected: boolean; onSelect(): void }) {
  const earned = state.badges[def.id];
  const ev = earned ? null : evaluate(def.rule, state, catalog);
  const hiddenLocked = def.hidden && !earned;
  const title = hiddenLocked ? "???" : def.name;
  return (
    <li>
      <button className={`badge-card ${earned ? "earned" : "locked"} ${selected ? "selected" : ""}`} onClick={onSelect} aria-pressed={selected}>
        <Medallion def={def} earned={!!earned} />
        <span className="bname">
          {title}
          {def.tier && !hiddenLocked && <span className={`tierchip tier-${def.tier}`}>{def.tier}</span>}
        </span>
        <span className="bdesc">
          {earned ? `Earned ${new Date(earned.earnedAt).toLocaleDateString()}` : hiddenLocked ? "Hidden badge. Keep exploring." : def.description}
        </span>
        {ev?.target !== undefined && !hiddenLocked && (
          <span className="bprogress" role="img" aria-label={`${ev.current} of ${ev.target}`}>
            <span className="bar">
              <span style={{ width: `${Math.min(100, ((ev.current ?? 0) / ev.target) * 100)}%` }} />
            </span>
            <span className="muted small">
              {ev.current}/{ev.target}
            </span>
          </span>
        )}
        <span className="sr">{earned ? "earned" : "not yet earned"}</span>
      </button>
    </li>
  );
}

function Detail({ def, state }: { def: BadgeDef; state: Progress }) {
  const earned = state.badges[def.id];
  const hiddenLocked = def.hidden && !earned;
  return (
    <section className="badge-detail" aria-live="polite">
      <Medallion def={def} earned={!!earned} />
      <div>
        <h3>{hiddenLocked ? "Hidden badge" : def.name}{def.tier && !hiddenLocked ? ` (${def.tier})` : ""}</h3>
        <p>{hiddenLocked ? "This badge is a secret. Do something memorable and it will appear." : def.description}</p>
        {earned ? (
          <>
            <p className="muted">Earned on {new Date(earned.earnedAt).toLocaleString()}.</p>
            {earned.evidence?.lessons && earned.evidence.lessons.length > 0 && (
              <>
                <p className="muted small">Counted towards this badge:</p>
                <ul className="evidence">
                  {earned.evidence.lessons.map((id) => {
                    const l = getLesson(id);
                    return <li key={id}>{l ? <Link to={`/lesson/${id}`}>{l.meta.title}</Link> : id}</li>;
                  })}
                </ul>
              </>
            )}
          </>
        ) : (
          !hiddenLocked && <p className="muted">Not earned yet.</p>
        )}
      </div>
    </section>
  );
}

export function Profile() {
  const state = useAllProgress();
  const [selectedId, setSelectedId] = useState<string | null>(null);
  const available = badgeDefs.filter((b) => isAvailable(b.rule, catalog));
  const soon = badgeDefs.filter((b) => !isAvailable(b.rule, catalog));
  const earnedCount = available.filter((b) => state.badges[b.id]).length;
  const completed = Object.values(state.lessons).filter((l) => l.status === "complete").length;
  const selected = badgeDefs.find((b) => b.id === selectedId);

  return (
    <main className="page profile">
      <h1>Profile</h1>
      <ul className="stats" aria-label="Your progress">
        <li><b>{completed}</b><span>of {catalog.lessons.length} lessons complete</span></li>
        <li><b>{state.streak.current}</b><span>day streak{state.streak.current === 1 ? "" : "s"} (best {state.streak.longest})</span></li>
        <li><b>{earnedCount}</b><span>of {available.length} badges earned</span></li>
      </ul>
      <p className="muted small">One missed day per week is forgiven, so a streak is never a source of pressure.</p>

      {selected && <Detail def={selected} state={state} />}

      {CATEGORIES.map((c) => {
        const list = available.filter((b) => b.category === c.id);
        if (list.length === 0) return null;
        return (
          <section key={c.id} className="badge-group">
            <h2>{c.title}</h2>
            <ul className="badge-grid">
              {list.map((b) => (
                <BadgeCard key={b.id} def={b} state={state} selected={selectedId === b.id} onSelect={() => setSelectedId(selectedId === b.id ? null : b.id)} />
              ))}
            </ul>
          </section>
        );
      })}

      {soon.length > 0 && (
        <details className="soon">
          <summary>{soon.length} more badges unlock as new modules arrive</summary>
          <ul className="badge-grid">
            {soon.map((b) => (
              <BadgeCard key={b.id} def={b} state={state} selected={selectedId === b.id} onSelect={() => setSelectedId(selectedId === b.id ? null : b.id)} />
            ))}
          </ul>
        </details>
      )}
    </main>
  );
}
