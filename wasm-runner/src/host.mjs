// Builds the `host` the driver talks to (see driver.mjs) from worker pools.
//   compilePool runs "compile" and "link" tasks; execPool runs "exec" tasks. They may be the same pool.
// Nothing here ever rejects: a failed or timed-out task becomes a failed result the driver can report.
const failure = (what, o) => `${what}: ${o.timedOut ? "timed out" : o.error}\n`;

export function makeHost({ compilePool, execPool = compilePool, compileTimeoutMs = 120000 }) {
  return {
    /** Compiled hidden-test objects, kept across runs (the tests do not change between a learner's attempts). */
    cache: new Map(),
    async compile(p) {
      const o = await compilePool.submit("compile", p, compileTimeoutMs);
      return o.result ?? { ok: false, output: failure("compilation failed", o) };
    },
    async link(p) {
      const o = await compilePool.submit("link", p, compileTimeoutMs);
      return o.result ?? { ok: false, output: failure("linking failed", o) };
    },
    async exec(p) {
      const { timeoutMs, ...payload } = p;
      const o = await execPool.submit("exec", payload, timeoutMs);
      if (o.result) return { ...o.result, timedOut: false };
      if (o.timedOut) return { stdout: "", stderr: "", exitCode: null, trap: null, truncated: false, timedOut: true };
      return { stdout: "", stderr: "", exitCode: null, trap: String(o.error).split("\n")[0], truncated: false, timedOut: false };
    },
  };
}
