// A `host` (see driver.mjs) for Node: worker threads, one pool for compiling and running.
import os from "node:os";
import { Worker } from "node:worker_threads";
import { runRequest } from "./driver.mjs";
import { makeHost } from "./host.mjs";
import { Pool } from "./pool.mjs";

const workerUrl = new URL("./node-worker.mjs", import.meta.url);
const workerData = {
  sysrootUrl: new URL("sysroot.tar", import.meta.resolve("browsercc")).href,
  headerDirUrl: new URL("../../runner-image/include/", import.meta.url).href,
};

function spawn(cb) {
  // 1 MB of native stack, like a browser's JS engine: code that needs more must not pass here either.
  const w = new Worker(workerUrl, { workerData, resourceLimits: { stackSizeMb: 1 } });
  w.on("message", cb.message);
  w.on("error", cb.error);
  w.on("exit", cb.exit);
  return {
    post: (m) => w.postMessage(m),
    terminate: () => {
      w.removeAllListeners();
      w.on("error", () => {});
      return w.terminate();
    },
  };
}

/**
 * @param {{workers?: number, compileTimeoutMs?: number}} [opts]
 * Each worker holds a copy of the toolchain (~100 MB), so the default is half the CPUs.
 */
export function createNodeRunner({ workers = Math.max(2, Math.floor(os.availableParallelism() / 2)), compileTimeoutMs = 120000 } = {}) {
  const pool = new Pool({ size: workers, spawn });
  const host = makeHost({ compilePool: pool, compileTimeoutMs });
  return { run: (req) => runRequest(host, req), close: () => pool.close(), host };
}
