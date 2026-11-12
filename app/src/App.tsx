import { Suspense, lazy, useEffect, useState } from "react";
import { HashRouter, Link, Route, Routes } from "react-router-dom";
import { badgeDefs, catalog } from "./badges/catalog";
import { isAvailable } from "./badges/engine";
import { ToastHost } from "./components/ToastHost";
import { Home } from "./pages/Home";
import { Profile } from "./pages/Profile";
import { Settings } from "./pages/Settings";
import { WarmupPage } from "./pages/Warmup";
import { allCards } from "./warmup/bank";
import { counts } from "./warmup/session";
import { localDate } from "./state/streak";
import { runner } from "./runner";
import { useAllProgress } from "./state/store";

// The lesson page pulls in Monaco (large), so load it only when a lesson is opened.
const LessonPage = lazy(() => import("./pages/Lesson").then((m) => ({ default: m.LessonPage })));

function RunnerStatus() {
  const [ok, setOk] = useState<boolean | null>(null);
  useEffect(() => {
    let live = true;
    const check = () => void runner.health().then((v) => live && setOk(v));
    check();
    const t = setInterval(check, 10_000);
    return () => {
      live = false;
      clearInterval(t);
    };
  }, []);
  const text = ok === null ? "checking…" : ok ? "online" : "offline";
  return (
    <span className={`status ${ok ? "on" : ok === false ? "off" : ""}`} role="status" title={ok === false ? "Start it with: docker compose up -d runner" : ""}>
      <span aria-hidden="true">{ok ? "●" : "○"}</span> {runner.label}: {text}
    </span>
  );
}

function Nav() {
  const state = useAllProgress();
  const earned = badgeDefs.filter((b) => isAvailable(b.rule, catalog) && state.badges[b.id]).length;
  const { due } = counts({ cards: allCards, reviews: state.reviews, completed: new Set(Object.entries(state.lessons).filter(([, p]) => p.status === "complete").map(([id]) => id)), today: localDate(new Date()) });
  return (
    <nav className="nav" aria-label="Main">
      <Link to="/warmup">
        Warm-up{due > 0 && <span className="count" aria-label={`${due} cards to review`}>{Math.min(99, due)}</span>}
      </Link>
      <Link to="/profile">
        Profile{earned > 0 && <span className="count" aria-label={`${earned} badges earned`}>{earned}</span>}
      </Link>
      <Link to="/settings">Settings</Link>
    </nav>
  );
}

export function App() {
  return (
    <HashRouter>
      <header className="topbar">
        <Link to="/" className="brand">
          CPP: <span className="z">Zero</span> to <span className="h">Hero</span>
        </Link>
        <Nav />
        <RunnerStatus />
      </header>
      <Routes>
        <Route path="/" element={<Home />} />
        <Route path="/profile" element={<Profile />} />
        <Route path="/settings" element={<Settings />} />
        <Route path="/warmup" element={<WarmupPage />} />
        <Route
          path="/lesson/:id"
          element={
            <Suspense
              fallback={
                <main className="page">
                  <p className="muted">Loading editor…</p>
                </main>
              }
            >
              <LessonPage />
            </Suspense>
          }
        />
        <Route
          path="*"
          element={
            <main className="page">
              <p>
                Not found. <Link to="/">Home</Link>
              </p>
            </main>
          }
        />
      </Routes>
      <ToastHost />
    </HashRouter>
  );
}
