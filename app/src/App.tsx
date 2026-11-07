import { Suspense, lazy, useEffect, useState } from "react";
import { HashRouter, Link, Route, Routes } from "react-router-dom";
import { Home } from "./pages/Home";
import { runner } from "./runner";

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

export function App() {
  return (
    <HashRouter>
      <header className="topbar">
        <Link to="/" className="brand">
          CPP: <span className="z">Zero</span> to <span className="h">Hero</span>
        </Link>
        <RunnerStatus />
      </header>
      <Routes>
        <Route path="/" element={<Home />} />
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
    </HashRouter>
  );
}
