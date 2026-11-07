import { RunnerError } from "./types";
import type { Capability, RunRequest, RunResult, Runner } from "./types";

/** Talks to the docker-compose runner-service (see /runner-service). Full C++ fidelity. */
export class ServerRunner implements Runner {
  readonly id = "server";
  readonly label = "Docker runner";
  readonly capabilities: ReadonlySet<Capability> = new Set<Capability>(["exceptions", "threads", "sanitizers"]);

  private readonly baseUrl: string;

  constructor(baseUrl: string) {
    this.baseUrl = baseUrl;
  }

  async run(req: RunRequest, signal?: AbortSignal): Promise<RunResult> {
    let res: Response;
    try {
      res = await fetch(`${this.baseUrl}/run`, {
        method: "POST",
        headers: { "content-type": "application/json" },
        body: JSON.stringify(req),
        signal,
      });
    } catch (e) {
      if (e instanceof DOMException && e.name === "AbortError") throw e;
      throw new RunnerError(`Cannot reach the runner at ${this.baseUrl}. Is it running? (docker compose up -d runner)`, "unreachable");
    }
    if (res.status === 503) throw new RunnerError("The runner is busy. Try again in a moment.", "busy");
    if (res.status === 429) throw new RunnerError("Too many runs. Wait a few seconds.", "busy");
    if (res.status === 400) {
      const body = (await res.json().catch(() => ({}))) as { issues?: string[] };
      throw new RunnerError(`Request rejected: ${(body.issues ?? []).join("; ") || "invalid"}`, "rejected");
    }
    if (!res.ok) throw new RunnerError(`Runner error (HTTP ${res.status}).`, "failed");
    return (await res.json()) as RunResult;
  }

  async health(): Promise<boolean> {
    try {
      const r = await fetch(`${this.baseUrl}/healthz`, { signal: AbortSignal.timeout(2000) });
      return r.ok;
    } catch {
      return false;
    }
  }
}
