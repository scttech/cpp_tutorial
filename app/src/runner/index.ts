import { ServerRunner } from "./serverRunner";
import { WasmRunner } from "./wasmRunner";
import type { Runner } from "./types";

// Build-time selection. The static GitHub Pages build sets VITE_RUNNER=wasm.
const kind = import.meta.env.VITE_RUNNER ?? "server";
const serverUrl = import.meta.env.VITE_RUNNER_URL ?? "http://localhost:8080";

export const runner: Runner = (() => {
  if (kind === "server") return new ServerRunner(serverUrl);
  if (kind === "wasm") return new WasmRunner();
  throw new Error(`Runner "${kind}" is not implemented yet`);
})();

export * from "./types";
