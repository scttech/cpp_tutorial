// Browser Worker entry (bundled by Vite). The toolchain's large files are separate assets, fetched on first use
// and kept in the HTTP cache.
import sysrootUrl from "browsercc/dist/sysroot.tar?url";
import zthTest from "../../runner-image/include/zth_test.hpp?raw";
import { createOps, messageHandler } from "./worker-core.mjs";

const ops = createOps({
  async loadSysroot() {
    const res = await fetch(sysrootUrl);
    if (!res.ok) throw new Error(`could not load the compiler's library files (HTTP ${res.status})`);
    return res.arrayBuffer();
  },
  async loadHeaders() {
    return { "zth_test.hpp": zthTest };
  },
});
self.onmessage = (e) => messageHandler(ops, (m) => self.postMessage(m))(e.data);
