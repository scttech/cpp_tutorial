/// <reference types="vite/client" />

interface ImportMetaEnv {
  readonly VITE_RUNNER?: "server" | "wasm";
  readonly VITE_RUNNER_URL?: string;
}
interface ImportMeta {
  readonly env: ImportMetaEnv;
}
