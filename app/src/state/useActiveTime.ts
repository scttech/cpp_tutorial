import { useEffect, useRef } from "react";
import { addLessonTime } from "./store";

/** How long after the last key press, click or scroll the learner still counts as working. */
export const IDLE_MS = 60_000;
const TICK_MS = 5_000;
const SAVE_MS = 30_000;

/**
 * Measures how long a lesson page is really in use: the tab visible and some input in the last minute. The time is saved every half minute,
 * when the page is hidden or left, and whenever `flush()` is called (before a run is recorded, so the time to the first pass is right).
 */
export function useActiveTime(lessonId: string): { flush(): void } {
  const pending = useRef(0);
  const last = useRef(Date.now());
  const lastInput = useRef(Date.now());
  const flushRef = useRef(() => undefined as void);

  useEffect(() => {
    const tick = () => {
      const now = Date.now();
      if (document.visibilityState === "visible" && now - lastInput.current < IDLE_MS) pending.current += now - last.current;
      last.current = now;
    };
    const flush = () => {
      tick();
      if (pending.current > 0) {
        addLessonTime(lessonId, pending.current);
        pending.current = 0;
      }
    };
    flushRef.current = flush;
    const touch = () => {
      lastInput.current = Date.now();
    };
    const onHide = () => {
      if (document.visibilityState === "hidden") flush();
    };
    const events = ["keydown", "pointerdown", "wheel", "scroll", "pointermove"] as const;
    for (const e of events) window.addEventListener(e, touch, { capture: true, passive: true });
    document.addEventListener("visibilitychange", onHide);
    window.addEventListener("pagehide", flush);
    const t1 = setInterval(tick, TICK_MS);
    const t2 = setInterval(flush, SAVE_MS);
    last.current = Date.now();
    return () => {
      clearInterval(t1);
      clearInterval(t2);
      for (const e of events) window.removeEventListener(e, touch, { capture: true });
      document.removeEventListener("visibilitychange", onHide);
      window.removeEventListener("pagehide", flush);
      flush();
    };
  }, [lessonId]);

  return { flush: () => flushRef.current() };
}
