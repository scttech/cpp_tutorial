// Main-thread side of the in-browser runner (bundled by Vite). Work happens in Workers so the page stays
// responsive during a compile, and so a runaway program can be stopped by terminating its Worker.
import clangWasmUrl from "browsercc/dist/clang.wasm?url";
import lldWasmUrl from "browsercc/dist/lld.wasm?url";
import sysrootUrl from "browsercc/dist/sysroot.tar?url";
import { runRequest } from "./driver.mjs";
import { makeHost } from "./host.mjs";
import { Pool } from "./pool.mjs";

function spawn(cb) {
  // The literal `new Worker(new URL(...))` form is what lets Vite find and bundle the Worker.
  const w = new Worker(new URL("./browser-worker.mjs", import.meta.url), { type: "module" });
  w.onmessage = (e) => cb.message(e.data);
  w.onerror = (e) => cb.error(e.message || "the compiler worker failed to start");
  w.onmessageerror = () => cb.error("a message between workers could not be decoded");
  return { post: (m) => w.postMessage(m), terminate: () => w.terminate() };
}

/**
 * @param {{compileWorkers?: number}} [opts]
 * Compiling needs up to two Clang instances at once (the learner's file and the hidden tests, the first time);
 * each instance can take hundreds of MB, so compile Workers are recycled after a few tasks.
 */
export function createBrowserRunner({ compileWorkers = 2 } = {}) {
  const make = () => {
    const compilePool = new Pool({ size: compileWorkers, spawn, maxJobsPerWorker: 6 });
    const execPool = new Pool({ size: 1, spawn });
    return { compilePool, execPool, host: makeHost({ compilePool, execPool }) };
  };
  let current = make();
  const cache = () => current.host.cache;
  return {
    /** @param {import("../../app/src/runner/types").RunRequest} req */
    run: (req) => runRequest(current.host, req),
    /** Stop everything that is running (the learner pressed Cancel). Compiled hidden tests are kept. */
    reset() {
      const kept = cache();
      void current.compilePool.close();
      void current.execPool.close();
      current = make();
      for (const [k, v] of kept) current.host.cache.set(k, v);
    },
    /** Download the toolchain into the HTTP cache ahead of the first Run. onProgress gets bytes so far and in total. */
    async warmUp(onProgress) {
      const urls = [clangWasmUrl, lldWasmUrl, sysrootUrl];
      const got = urls.map(() => 0), size = urls.map(() => 0);
      const report = () => onProgress?.({ loaded: got.reduce((a, b) => a + b, 0), total: size.reduce((a, b) => a + b, 0) });
      await Promise.all(urls.map(async (u, i) => {
        const res = await fetch(u);
        if (!res.ok || !res.body) throw new Error(`could not download the compiler (HTTP ${res.status})`);
        size[i] = Number(res.headers.get("content-length")) || 0;
        const reader = res.body.getReader();
        for (;;) {
          const { done, value } = await reader.read();
          if (done) break;
          got[i] += value.length;
          report();
        }
      }));
    },
  };
}
