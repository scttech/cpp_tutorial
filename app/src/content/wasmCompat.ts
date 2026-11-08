import report from "../../../content/wasm-compat.json";

// content/wasm-compat.json is written by tools/validate-content/compat-wasm.mjs (npm run compat): which lessons
// the in-browser runner can teach. Only consulted when the app is built with the WASM runner.
interface Entry {
  wasm: boolean;
  reason?: string;
}
const lessons = (report as { lessons: Record<string, Entry> }).lessons;

/** Why this lesson cannot run in the browser, or null if it can. A lesson nobody has checked yet is treated as unsupported. */
export function wasmProblem(lessonId: string): string | null {
  const e = lessons[lessonId];
  if (!e) return "it has not been checked against the in-browser compiler yet";
  return e.wasm ? null : (e.reason ?? "it does not run in the browser");
}
