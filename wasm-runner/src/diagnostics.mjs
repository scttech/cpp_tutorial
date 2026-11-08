// Compiler and linker output -> structured diagnostics, in the same shape the Docker runner returns.
// The compiler part is a port of runner-service/src/diagnostics.ts (keep them in sync).

// path:line:col: severity: message
const COMPILER = /^(.+?):(\d+):(\d+): (fatal error|error|warning|note): (.*)$/;
// GNU ld style: file.cpp:(.text+0x15): undefined reference to `f(int)'
const LINKER = /^([^\s:]+):\(\.text[^)]*\): ((?:undefined reference to|multiple definition of) .+)$/;
// clang++: error: linker command failed with exit code 1 (use -v to see invocation)
const LINKER_FAILED = /^clang\+\+: error: (linker command failed.*)$/;

export function parseDiagnostics(raw) {
  const out = [];
  for (const line of raw.split("\n")) {
    const m = COMPILER.exec(line);
    if (m) {
      const [, file, ln, col, sev, message] = m;
      out.push({
        severity: sev === "warning" ? "warning" : sev === "note" ? "note" : "error",
        file: file.replace(/^\/work\//, ""),
        line: Number(ln),
        column: Number(col),
        message,
      });
      continue;
    }
    const l = LINKER.exec(line);
    if (l) { out.push({ severity: "error", file: l[1], line: 0, column: 0, message: l[2] }); continue; }
    const f = LINKER_FAILED.exec(line);
    if (f) out.push({ severity: "error", file: "(linker)", line: 0, column: 0, message: f[1] });
  }
  return out;
}

/**
 * wasm-ld words its errors differently from GNU ld. Rewrite them in GNU form, naming the source file instead
 * of the temporary object, so the same parser and the same friendly-error rules apply to both runners.
 * @param {string} output   wasm-ld's stderr
 * @param {string[]} unitNames  source file of each linked object, in link order
 */
export function normalizeLinkerOutput(output, unitNames) {
  const unit = (obj) => unitNames[Number(/link-(\d+)\.o/.exec(obj)?.[1])] ?? obj;
  const lines = output.split("\n");
  const out = [];
  let failed = false;
  for (let i = 0; i < lines.length; i++) {
    const line = lines[i];
    let m = /^wasm-ld: error: (\S+\.o): undefined symbol: (.+)$/.exec(line);
    if (m) { out.push(`${unit(m[1])}:(.text+0x0): undefined reference to \`${m[2]}'`); failed = true; continue; }
    m = /^wasm-ld: error: duplicate symbol: (.+)$/.exec(line);
    if (m) {
      const where = [];
      while (lines[i + 1]?.startsWith(">>> defined in ")) where.push(unit(lines[++i].slice(">>> defined in ".length).trim()));
      out.push(`${where[1] ?? where[0] ?? "(link)"}:(.text+0x0): multiple definition of \`${m[1]}'; ${where[0] ?? "(link)"}:(.text+0x0): first defined here`);
      failed = true;
      continue;
    }
    if (/^wasm-ld: error:/.test(line)) failed = true;
    out.push(line);
  }
  const text = out.join("\n");
  return failed ? `${text.replace(/\n*$/, "\n")}clang++: error: linker command failed with exit code 1 (use -v to see invocation)\n` : text;
}
