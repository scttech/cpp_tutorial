// Runs ONE instance of a linked program under WASI. No timers here: the host enforces time limits by
// terminating the worker this runs in (a synchronous infinite loop cannot be interrupted from inside).
import { WASI, File, OpenFile, ConsoleStdout, PreopenDirectory } from "@bjorn3/browser_wasi_shim";

class OutputLimit extends Error {}

const enc = new TextEncoder();

function capped(limit) {
  const chunks = [];
  let size = 0;
  const sink = {
    truncated: false,
    fd: new ConsoleStdout((data) => {
      if (size + data.length > limit) {
        sink.truncated = true;
        data = data.subarray(0, Math.max(0, limit - size));
        chunks.push(data.slice());
        size += data.length;
        throw new OutputLimit("output limit reached"); // stops a program that prints without end
      }
      chunks.push(data.slice());
      size += data.length;
    }),
    text() {
      const all = new Uint8Array(size);
      let o = 0;
      for (const c of chunks) { all.set(c, o); o += c.length; }
      return new TextDecoder("utf-8", { fatal: false }).decode(all) + (sink.truncated ? "\n[output truncated]" : "");
    },
  };
  return sink;
}

/**
 * @param {object} o
 * @param {WebAssembly.Module} o.module
 * @param {string[]} [o.args]            argv after the program name
 * @param {string}   [o.stdin]
 * @param {Record<string,string>} [o.files]  the learner's files, visible in the working directory
 * @param {number}   [o.maxOutputBytes]
 * @returns {Promise<{stdout:string, stderr:string, exitCode:number|null, trap:string|null, truncated:boolean}>}
 */
export async function execProgram({ module, args = [], stdin = "", files = {}, maxOutputBytes = 65536 }) {
  const out = capped(maxOutputBytes), err = capped(maxOutputBytes);
  const dir = new Map(Object.entries(files).map(([n, c]) => [n, new File(enc.encode(c))]));
  const fds = [new OpenFile(new File(enc.encode(stdin))), out.fd, err.fd, new PreopenDirectory(".", dir)];
  const wasi = new WASI(["a.out", ...args], [], fds, { debug: false });
  let exitCode = null, trap = null;
  try {
    const instance = await WebAssembly.instantiate(module, { wasi_snapshot_preview1: wasi.wasiImport });
    exitCode = wasi.start(instance);
  } catch (e) {
    if (!(e instanceof OutputLimit)) trap = describeTrap(e);
  }
  return { stdout: out.text(), stderr: err.text(), exitCode, trap, truncated: out.truncated || err.truncated };
}

/** A short, learner-readable reason for a trap (the WebAssembly equivalent of a crash). */
export function describeTrap(e) {
  const m = String(e?.message ?? e);
  if (/unreachable/i.test(m)) return "the program aborted";
  if (/out of bounds/i.test(m)) return "invalid memory access (out of bounds)";
  if (/call stack|stack overflow/i.test(m) || e instanceof RangeError) return "stack overflow (recursion too deep?)";
  if (/divide by zero|divisible by zero/i.test(m)) return "integer division by zero";
  if (/integer overflow|float unrepresentable/i.test(m)) return "invalid numeric conversion";
  return m.split("\n")[0].slice(0, 160);
}
