import { ServerRunner } from "./serverRunner";
import type { Runner } from "./types";

// Build-time selection. The static GitHub Pages build will set VITE_RUNNER=wasm.
const kind = import.meta.env.VITE_RUNNER ?? "server";
const serverUrl = import.meta.env.VITE_RUNNER_URL ?? "http://localhost:8080";

export const runner: Runner = (() => {
  if (kind === "server") return new ServerRunner(serverUrl);
  throw new Error(`Runner "${kind}" is not implemented yet`);
})();

export * from "./types";
