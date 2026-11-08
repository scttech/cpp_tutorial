// Worker-thread side of the Node host.
import { parentPort, workerData } from "node:worker_threads";
import { readFile, readdir } from "node:fs/promises";
import { fileURLToPath } from "node:url";
import { createOps, messageHandler } from "./worker-core.mjs";

// browsercc loads its .wasm files with fetch(); Node's fetch cannot read file: URLs.
const realFetch = globalThis.fetch;
globalThis.fetch = async (u, ...r) =>
  String(u).startsWith("file:") ? new Response(await readFile(fileURLToPath(u))) : realFetch(u, ...r);

const ops = createOps({
  async loadSysroot() {
    const tar = await readFile(fileURLToPath(workerData.sysrootUrl));
    return tar.buffer.slice(tar.byteOffset, tar.byteOffset + tar.byteLength);
  },
  async loadHeaders() {
    const dir = fileURLToPath(workerData.headerDirUrl);
    const headers = {};
    for (const f of await readdir(dir)) headers[f] = await readFile(`${dir}/${f}`, "utf8");
    return headers;
  },
});
parentPort.on("message", messageHandler(ops, (m) => parentPort.postMessage(m)));
