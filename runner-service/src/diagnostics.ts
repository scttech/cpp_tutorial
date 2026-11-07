import type { CompileDiagnostic } from "./types.js";

// Matches Clang/GCC style: path:line:col: severity: message
const LINE = /^(.+?):(\d+):(\d+): (fatal error|error|warning|note): (.*)$/;

/** Parse raw compiler stderr into structured diagnostics. Unparseable lines are skipped. */
export function parseDiagnostics(raw: string): CompileDiagnostic[] {
  const out: CompileDiagnostic[] = [];
  for (const line of raw.split("\n")) {
    const m = LINE.exec(line);
    if (!m) continue;
    const [, file, ln, col, sev, message] = m;
    out.push({
      severity: sev === "warning" ? "warning" : sev === "note" ? "note" : "error",
      file: file!.replace(/^\/work\//, ""),
      line: Number(ln),
      column: Number(col),
      message: message!,
    });
  }
  return out;
}
