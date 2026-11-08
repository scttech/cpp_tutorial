import { createBrowserRunner } from "../../../wasm-runner/src/browser-host.mjs";
import type { Capability, RunRequest, RunResult, Runner, WarmUpProgress } from "./types";

/**
 * Compiles and runs C++ in the browser (Clang/LLD as WebAssembly, see /wasm-runner). Used by the static
 * GitHub Pages build. No exceptions, threads or sanitizers; content/wasm-compat.json says which lessons work.
 */
export class WasmRunner implements Runner {
  readonly id = "wasm";
  readonly label = "In-browser compiler";
  readonly capabilities: ReadonlySet<Capability> = new Set<Capability>();

  private readonly inner = createBrowserRunner();
  private warm: Promise<void> | null = null;

  async run(req: RunRequest, signal?: AbortSignal): Promise<RunResult> {
    if (signal?.aborted) throw new DOMException("Run cancelled", "AbortError");
    let onAbort: (() => void) | undefined;
    const aborted = new Promise<never>((_, reject) => {
      onAbort = () => {
        this.inner.reset(); // stops compilers and running programs by terminating their workers
        reject(new DOMException("Run cancelled", "AbortError"));
      };
      signal?.addEventListener("abort", onAbort, { once: true });
    });
    try {
      return (await Promise.race([this.inner.run(req), aborted])) as RunResult;
    } finally {
      if (onAbort) signal?.removeEventListener("abort", onAbort);
    }
  }

  async health(): Promise<boolean> {
    return typeof Worker !== "undefined" && typeof WebAssembly !== "undefined";
  }

  private readonly listeners = new Set<(p: WarmUpProgress) => void>();
  private lastProgress: WarmUpProgress | null = null;

  /** One download no matter how many pages ask; every asker hears the progress. */
  warmUp(onProgress?: (p: WarmUpProgress) => void): Promise<void> {
    if (onProgress) {
      this.listeners.add(onProgress);
      if (this.lastProgress) onProgress(this.lastProgress);
    }
    this.warm ??= this.inner
      .warmUp((p: WarmUpProgress) => {
        this.lastProgress = p;
        this.listeners.forEach((l) => l(p));
      })
      .catch((e: unknown) => {
        this.warm = null; // allow a retry on the next visit
        throw e;
      });
    return this.warm.finally(() => {
      if (onProgress) this.listeners.delete(onProgress);
    });
  }
}
