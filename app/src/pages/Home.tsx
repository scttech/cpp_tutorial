import { Link } from "react-router-dom";
import { modules } from "../content/loader";
import type { Lesson } from "../content/types";
import { runner } from "../runner";
import { useAllProgress } from "../state/progress";

export const isRunnable = (l: Lesson) => (l.meta.requires ?? []).every((c) => runner.capabilities.has(c));
export const missingCaps = (l: Lesson) => (l.meta.requires ?? []).filter((c) => !runner.capabilities.has(c));

export function Home() {
  const progress = useAllProgress();
  return (
    <main className="home">
      <img src={`${import.meta.env.BASE_URL}logo.png`} alt="CPP: Zero to Hero. Learn C++, build skills, become a hero." className="logo" />
      {modules.map((m) => {
        const done = m.lessons.filter((l) => progress.lessons[l.meta.id]?.status === "complete").length;
        return (
          <section key={m.id} className="module">
            <h2>
              {m.title} <span className="muted small">{done}/{m.lessons.length} complete</span>
            </h2>
            {m.summary && <p className="muted">{m.summary}</p>}
            <ul className="lesson-list">
              {m.lessons.map((l) => {
                const st = progress.lessons[l.meta.id]?.status ?? "available";
                if (!isRunnable(l)) {
                  return (
                    <li key={l.meta.id} className="locked" title={`Needs the full Docker runner: ${missingCaps(l).join(", ")}`}>
                      <span>{l.order}. {l.meta.title}</span>
                      <span className="badge">Server needed</span>
                    </li>
                  );
                }
                return (
                  <li key={l.meta.id}>
                    <Link to={`/lesson/${l.meta.id}`}>{l.order}. {l.meta.title}</Link>
                    <span className="muted small">{l.meta.estimatedMinutes ? `${l.meta.estimatedMinutes} min` : ""}</span>
                    {st === "complete" ? (
                      <span className="badge ok">✔ complete</span>
                    ) : st === "in_progress" ? (
                      <span className="badge">in progress</span>
                    ) : null}
                  </li>
                );
              })}
            </ul>
          </section>
        );
      })}
    </main>
  );
}
