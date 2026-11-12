import { useMemo } from "react";
import { Link } from "react-router-dom";
import { getLesson } from "../content/loader";
import { friendlyRuleInfo } from "../diagnostics/friendly";
import { explainReport } from "../diagnostics/runtimeReports";
import type { RuntimeReport } from "../runner/types";
import { useAllProgress } from "../state/store";
import { accuracyByTopic, effortRanking, formatDuration, heatmap, summarize, topErrors, weekly } from "../state/stats";
import type { ErrorKind } from "../state/stats";
import { localDate } from "../state/streak";
import { cardById } from "../warmup/bank";

const pct = (x: number) => `${Math.round(x * 100)}%`;
const titleOf = (id: string) => getLesson(id)?.meta.title ?? id;

/** A few plain words for a rule's explanation: no code marks, no placeholders, the first sentence. */
function shorten(text: string): string {
  const plain = text.replace(/`/g, "").replace(/\$\d/g, "…").replace(/\s+/g, " ").trim();
  const first = /^.*?[.!?](\s|$)/.exec(plain)?.[0].trim() ?? plain;
  return first.length > 140 ? `${first.slice(0, 137)}…` : first;
}

/** What a recorded problem is called, and the lesson that explains it (when there is one). */
function describeError(e: ErrorKind): { label: string; lesson?: string } {
  if (e.kind === "compile") {
    if (e.id === "other") return { label: "Other compiler errors (no plain-language explanation yet)" };
    const info = friendlyRuleInfo(e.id);
    return { label: info ? shorten(info.friendly) : e.id, lesson: info?.lesson };
  }
  const why = explainReport({ tool: "AddressSanitizer", kind: "", summary: e.id, frames: [] } as RuntimeReport);
  return { label: why ? `${why.title} (${e.id})` : e.id, lesson: why?.lesson ?? undefined };
}

function Tile({ label, value, note }: { label: string; value: string; note?: string }) {
  return (
    <div className="stat-tile">
      <dt className="muted small">{label}</dt>
      <dd>
        <strong>{value}</strong>
        {note && <span className="muted small"> {note}</span>}
      </dd>
    </div>
  );
}

export function Stats() {
  const progress = useAllProgress();
  const today = localDate(new Date());
  const sum = useMemo(() => summarize(progress), [progress]);
  const week = useMemo(() => weekly(progress, today), [progress, today]);
  const heat = useMemo(() => heatmap(progress, today, new Date().getDay()), [progress, today]);
  const effort = useMemo(() => effortRanking(progress), [progress]);
  const errors = useMemo(() => topErrors(progress), [progress]);
  const accuracy = useMemo(() => accuracyByTopic(progress, (id) => cardById.get(id)?.topic), [progress]);
  const answered = accuracy.reduce((n, a) => n + a.seen, 0);
  const right = accuracy.reduce((n, a) => n + a.correct, 0);
  const rows: [string, (a: typeof week.thisWeek) => string][] = [
    ["Time on lessons", (a) => (a.ms > 0 ? formatDuration(a.ms) : "–")],
    ["Test and plain runs", (a) => String(a.runs)],
    ["Warm-up cards", (a) => String(a.warmups)],
    ["Re-solves", (a) => String(a.resolves)],
    ["Lessons completed", (a) => String(a.lessons)],
  ];

  return (
    <main className="page stats-page">
      <h1>Learning stats</h1>
      <p className="muted">Worked out from your saved progress. It all stays in this browser.</p>

      <section aria-labelledby="st-sum">
        <h2 id="st-sum">At a glance</h2>
        <dl className="stat-tiles">
          <Tile label="Lessons completed" value={String(sum.completed)} />
          <Tile label="Passed on the first try" value={sum.firstTryRate === null ? "–" : pct(sum.firstTryRate)} note={sum.measured ? `of ${sum.measured} measured` : ""} />
          <Tile label="Typical test runs to pass" value={sum.medianTries === null ? "–" : String(sum.medianTries)} note="(median)" />
          <Tile label="Time on lessons" value={sum.totalMs > 0 ? formatDuration(sum.totalMs) : "–"} />
          <Tile label="Streak" value={`${sum.streak.current} day${sum.streak.current === 1 ? "" : "s"}`} note={`best ${sum.streak.longest}`} />
          <Tile label="Warm-up accuracy" value={answered ? pct(right / answered) : "–"} note={answered ? `${answered} answers` : ""} />
        </dl>
      </section>

      <section aria-labelledby="st-act">
        <h2 id="st-act">Activity, last 12 weeks</h2>
        <ul className="heat" aria-label="Activity per day, oldest first">
          {heat.flat().map((d) => (
            <li key={d.date} className={`lv${d.level}${d.future ? " future" : ""}`} title={d.future ? undefined : `${d.date}: ${d.text}`} aria-hidden={d.future ? true : undefined}>
              {!d.future && <span className="sr">{d.date}: {d.text}</span>}
            </li>
          ))}
        </ul>
        <p className="muted small heat-legend" aria-hidden="true">
          Less <span className="swatch lv0" /> <span className="swatch lv1" /> <span className="swatch lv2" /> <span className="swatch lv3" /> <span className="swatch lv4" /> More
        </p>
        <table className="stat-table">
          <caption className="sr">This week compared with last week</caption>
          <thead>
            <tr>
              <th scope="col"></th>
              <th scope="col">Last 7 days</th>
              <th scope="col">The 7 before</th>
            </tr>
          </thead>
          <tbody>
            {rows.map(([label, f]) => (
              <tr key={label}>
                <th scope="row">{label}</th>
                <td>{f(week.thisWeek)}</td>
                <td>{f(week.lastWeek)}</td>
              </tr>
            ))}
          </tbody>
        </table>
      </section>

      <section aria-labelledby="st-effort">
        <h2 id="st-effort">Lessons that took the most effort</h2>
        {effort.length === 0 ? (
          <p className="muted">Nothing stands out yet. Lessons that took many test runs, a long time, hints or the solution will show here.</p>
        ) : (
          <table className="stat-table">
            <thead>
              <tr>
                <th scope="col">Lesson</th>
                <th scope="col">Test runs to pass</th>
                <th scope="col">Time to pass</th>
                <th scope="col">Hints</th>
                <th scope="col">Solution</th>
              </tr>
            </thead>
            <tbody>
              {effort.map((e) => (
                <tr key={e.lessonId}>
                  <th scope="row">
                    <Link to={`/lesson/${e.lessonId}`}>{titleOf(e.lessonId)}</Link>
                  </th>
                  <td>{e.tries ?? "–"}</td>
                  <td>{e.ms ? formatDuration(e.ms) : "–"}</td>
                  <td>{e.hints}</td>
                  <td>{e.revealed ? "looked" : "–"}</td>
                </tr>
              ))}
            </tbody>
          </table>
        )}
      </section>

      <section aria-labelledby="st-err">
        <h2 id="st-err">Problems you hit most</h2>
        {errors.length === 0 ? (
          <p className="muted">No compiler errors or sanitizer reports recorded yet.</p>
        ) : (
          <ol className="stat-list">
            {errors.map((e) => {
              const d = describeError(e);
              return (
                <li key={e.key}>
                  <span className="stat-count" aria-label={`${e.count} times`}>
                    {e.count}×
                  </span>
                  <span className="stat-main">
                    {d.label}
                    {d.lesson && getLesson(d.lesson) && (
                      <>
                        {" "}
                        <Link to={`/lesson/${d.lesson}`}>Learn more</Link>
                      </>
                    )}
                  </span>
                  <span className="muted small">last {e.lastAt.slice(0, 10)}</span>
                </li>
              );
            })}
          </ol>
        )}
      </section>

      <section aria-labelledby="st-acc">
        <h2 id="st-acc">Warm-up accuracy by topic</h2>
        {accuracy.length === 0 ? (
          <p className="muted">
            Answer some <Link to="/warmup">warm-up cards</Link> and your accuracy for each topic will show here.
          </p>
        ) : (
          <ul className="stat-list">
            {accuracy.map((a) => (
              <li key={a.topic}>
                <span className="stat-main">{a.topic}</span>
                <meter min={0} max={1} low={0.5} high={0.8} optimum={1} value={a.rate} aria-label={`${a.topic} accuracy`} />
                <span className="stat-num">
                  {pct(a.rate)} <span className="muted small">({a.correct} of {a.seen})</span>
                </span>
              </li>
            ))}
          </ul>
        )}
      </section>

      <p className="muted small">Time on lessons and the problem tallies are recorded from the day they were added to the app, so older lessons show a dash.</p>
    </main>
  );
}
