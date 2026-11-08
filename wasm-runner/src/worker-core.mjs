// The tasks a worker can do, shared by the Node worker thread and the browser Worker.
// The environment supplies how to load the (large) sysroot and the harness headers.
import { Toolchain } from "./toolchain.mjs";
import { execProgram } from "./exec.mjs";

/**
 * @param {{loadSysroot: () => Promise<ArrayBuffer>, loadHeaders: () => Promise<Record<string,string>>}} env
 */
export function createOps({ loadSysroot, loadHeaders }) {
  let toolchain = null;
  const getToolchain = async () => {
    toolchain ??= new Toolchain({ sysroot: await loadSysroot(), systemHeaders: await loadHeaders() });
    return toolchain;
  };
  return {
    compile: async (p) => (await getToolchain()).compileUnit(p),
    async link(p) {
      const r = await (await getToolchain()).link(p);
      if (!r.ok) return r;
      return { ok: true, output: r.output, module: await WebAssembly.compile(r.bytes) };
    },
    exec: (p) => execProgram(p),
  };
}

/** Wires `ops` to a message endpoint with `post(msg)`; returns the handler for incoming messages. */
export function messageHandler(ops, post) {
  return async ({ id, op, payload }) => {
    try {
      post({ id, ok: true, result: await ops[op](payload) });
    } catch (e) {
      post({ id, ok: false, error: String(e?.stack ?? e) });
    }
  };
}
