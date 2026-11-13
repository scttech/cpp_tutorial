import { useCallback, useEffect, useMemo, useState } from "react";
import { Link, useSearchParams } from "react-router-dom";
import { FlagButton } from "../components/FlagButton";
import { DemoBlock } from "../components/GotchaList";
import { Markdown } from "../components/Markdown";
import { getState, recordReview, useAllProgress } from "../state/store";
import { localDate } from "../state/streak";
import { allCards } from "../warmup/bank";
import type { Card } from "../warmup/cards";
import { isDue } from "../warmup/schedule";
import { counts, eligible, pickSession, SESSION_SIZE } from "../warmup/session";
import { practiceSet, weakSpotsFor } from "../warmup/weakView";

const completedLessons = () => new Set(Object.entries(getState().lessons).filter(([, p]) => p.status === "complete").map(([id]) => id));

/** Today's set, or (when `weak` is given: "all" or one unit) a focused practice set from the weak spots. */
const newSet = (weak: string | null) =>
  weak ? practiceSet(getState(), weak) : pickSession({ cards: allCards, reviews: getState().reviews, completed: completedLessons(), today: localDate(new Date()) });

/** The earliest date a card of the deck becomes due again (null when nothing has been asked yet). */
function nextDueDate(today: string): string | null {
  const dates = eligible(allCards, completedLessons())
    .map((c) => getState().reviews[c.id])
    .filter((r) => r !== undefined && !isDue(r, today))
    .map((r) => (r as { due: string }).due)
    .sort();
  return dates[0] ?? null;
}

interface Result {
  id: string;
  correct: boolean;
}

export function WarmupPage() {
  useAllProgress(); // re-render when a review is recorded
  const weak = useSearchParams()[0].get("weak");
  const [session, setSession] = useState<Card[]>(() => newSet(weak));
  const [index, setIndex] = useState(0);
  const [picked, setPicked] = useState<number | null>(null);
  const [checked, setChecked] = useState(false);
  const [results, setResults] = useState<Result[]>([]);

  const card = session[index];
  const finished = session.length > 0 && index >= session.length;

  const check = useCallback(
    (choice: number | null) => {
      if (!card || checked) return;
      const correct = choice !== null && choice === card.answer;
      setPicked(choice);
      setChecked(true);
      setResults((r) => [...r, { id: card.id, correct }]);
      recordReview(card.id, correct);
    },
    [card, checked],
  );
  const next = useCallback(() => {
    setIndex((i) => i + 1);
    setPicked(null);
    setChecked(false);
  }, []);
  const another = () => {
    setSession(newSet(weak));
    setIndex(0);
    setPicked(null);
    setChecked(false);
    setResults([]);
  };

  // keyboard: 1..5 pick a choice, Enter checks the answer and then goes on
  useEffect(() => {
    const onKey = (e: KeyboardEvent) => {
      if (!card || e.altKey || e.ctrlKey || e.metaKey) return;
      const t = e.target as HTMLElement | null;
      if (t && (t.tagName === "TEXTAREA" || t.tagName === "INPUT" && (t as HTMLInputElement).type !== "radio")) return;
      if (!checked && /^[1-9]$/.test(e.key) && Number(e.key) <= card.choices.length) {
        setPicked(Number(e.key) - 1);
      } else if (e.key === "Enter" && t?.tagName !== "BUTTON") {
        if (checked) next();
        else if (picked !== null) check(picked);
      }
    };
    window.addEventListener("keydown", onKey);
    return () => window.removeEventListener("keydown", onKey);
  }, [card, checked, picked, check, next]);

  const today = localDate(new Date());
  const summary = useMemo(() => ({ right: results.filter((r) => r.correct).length, total: results.length }), [results]);

  const title = weak ? "Weak-spot practice" : "Warm-up";
  if (session.length === 0 && weak) {
    return (
      <main className="page warmup">
        <h1>{title}</h1>
        <p className="banner ok" role="status">
          No weak spots right now. Nice work.
        </p>
        <Link className="btn" to="/">
          Back to the course
        </Link>
      </main>
    );
  }
  if (session.length === 0) {
    const due = nextDueDate(today);
    return (
      <main className="page warmup">
        <h1>Warm-up</h1>
        <p className="banner ok" role="status">
          You are all caught up.
          {due ? ` The next review is due on ${due}.` : ""} Finish more lessons and new gotchas to review will appear here.
        </p>
        <Link className="btn" to="/">
          Back to the course
        </Link>
      </main>
    );
  }

  if (finished) {
    const still = counts({ cards: allCards, reviews: getState().reviews, completed: completedLessons(), today, size: SESSION_SIZE });
    const more = weak ? weakSpotsFor(getState()).length > 0 : still.due + still.fresh > 0;
    return (
      <main className="page warmup">
        <h1>{title} done</h1>
        <p className="banner ok" role="status">
          {summary.right} of {summary.total} correct. {summary.right === summary.total ? "Nothing slipped." : "The ones you missed come back tomorrow."}
        </p>
        <div className="row">
          {more && (
            <button className="btn primary" onClick={another}>
              Another set
            </button>
          )}
          <Link className="btn" to="/">
            Back to the course
          </Link>
        </div>
      </main>
    );
  }
  if (!card) return null;

  const right = picked === card.answer;
  return (
    <main className="page warmup">
      <div className="warm-head">
        <h1>{title}</h1>
        <span className="muted">
          Card {index + 1} of {session.length} · {card.topic}
        </span>
      </div>
      <p className="warm-prompt">{card.prompt}</p>
      {card.code && <pre className="console warm-code">{card.code}</pre>}
      <fieldset className="choices" disabled={checked}>
        <legend className="sr">Choose an answer</legend>
        {card.choices.map((c, i) => {
          const cls = ["choice", picked === i ? "picked" : "", checked && i === card.answer ? "is-right" : "", checked && picked === i && i !== card.answer ? "is-wrong" : ""].join(" ").trim();
          return (
            <label key={i} className={cls}>
              <input type="radio" name="choice" checked={picked === i} onChange={() => setPicked(i)} />
              <span className="key" aria-hidden="true">
                {i + 1}
              </span>
              {card.mono ? <pre className="choice-code">{c}</pre> : <span>{c}</span>}
              {checked && i === card.answer && <span className="mark" aria-label="correct answer"> ✔</span>}
              {checked && picked === i && i !== card.answer && <span className="mark" aria-label="your answer, wrong"> ✘</span>}
            </label>
          );
        })}
      </fieldset>
      {!checked ? (
        <div className="row">
          <button className="btn primary" onClick={() => check(picked)} disabled={picked === null}>
            Check
          </button>
          <button className="btn" onClick={() => check(null)}>
            I don&rsquo;t know
          </button>
          <FlagButton target={{ type: "card", id: card.id }} label="Flag this card" align="left" collect={() => ({ context: `${card.topic}: ${card.prompt.slice(0, 120)}` })} />
        </div>
      ) : (
        <>
          <p className={right ? "banner ok" : "banner error"} role="status">
            {right ? "Correct." : picked === null ? "No problem. This one comes back tomorrow." : "Not quite. This one comes back tomorrow."}
          </p>
          <div className="warm-explain">
            <Markdown>{card.explain}</Markdown>
            {card.demo && <DemoBlock demo={card.demo} defaultOpen />}
            {card.lessonId && (
              <p className="muted small">
                From <Link to={`/lesson/${card.lessonId}`}>its lesson</Link>.
              </p>
            )}
          </div>
          <div className="row">
            <button className="btn primary" onClick={next}>
              {index + 1 < session.length ? "Next card" : "Finish"}
            </button>
            <FlagButton target={{ type: "card", id: card.id }} label="Flag this card" align="left" collect={() => ({ context: `${card.topic}: ${card.prompt.slice(0, 120)}` })} />
          </div>
        </>
      )}
    </main>
  );
}
