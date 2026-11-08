import { useEffect, useState } from "react";
import { runner } from ".";
import type { WarmUpProgress } from "./types";

export type ToolchainState =
  | { phase: "ready" }
  | { phase: "downloading"; progress: WarmUpProgress }
  | { phase: "failed"; message: string };

/** Starts fetching the in-browser compiler (if this runner needs one) and reports how far along it is. */
export function useToolchain(): ToolchainState {
  const [state, setState] = useState<ToolchainState>(runner.warmUp ? { phase: "downloading", progress: { loaded: 0, total: 0 } } : { phase: "ready" });
  useEffect(() => {
    if (!runner.warmUp) return;
    let alive = true;
    runner
      .warmUp((progress) => alive && setState({ phase: "downloading", progress }))
      .then(() => alive && setState({ phase: "ready" }))
      .catch((e: unknown) => alive && setState({ phase: "failed", message: e instanceof Error ? e.message : String(e) }));
    return () => {
      alive = false;
    };
  }, []);
  return state;
}
