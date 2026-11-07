import type { CompileDiagnostic } from "./types.js";

// Compiler: path:line:col: severity: message
const COMPILER = /^(.+?):(\d+):(\d+): (fatal error|error|warning|note): (.*)$/;
// Linker (GNU ld): file.cpp:(.text+0x15): undefined reference to `f(int)'
const LINKER = /^([^\s:]+):\(\.text[^)]*\): ((?:undefined reference to|multiple definition of) .+)$/;
// clang++: error: linker command failed with exit code 1 (use -v to see invocation)
const LINKER_FAILED = /^clang\+\+: error: (linker command failed.*)$/;

/**
 * Parse raw compiler/linker stderr into structured diagnostics.
 * Linker problems have no line number, so they are reported with line 0 (the UI shows them without a
 * squiggle). Lines that are neither are skipped.
 */
export function parseDiagnostics(raw: string): CompileDiagnostic[] {
  const out: CompileDiagnostic[] = [];
  for (const line of raw.split("\n")) {
    const m = COMPILER.exec(line);
    if (m) {
      const [, file, ln, col, sev, message] = m;
      out.push({
        severity: sev === "warning" ? "warning" : sev === "note" ? "note" : "error",
        file: file!.replace(/^\/work\//, ""),
        line: Number(ln),
        column: Number(col),
        message: message!,
      });
      continue;
    }
    const l = LINKER.exec(line);
    if (l) {
      out.push({ severity: "error", file: l[1]!, line: 0, column: 0, message: l[2]! });
      continue;
    }
    const f = LINKER_FAILED.exec(line);
    if (f) out.push({ severity: "error", file: "(linker)", line: 0, column: 0, message: f[1]! });
  }
  return out;
}
