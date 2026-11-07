import Docker from "dockerode";
import type { Config } from "./config.js";
import type { Sandbox, SandboxJob, SandboxOutput } from "./types.js";

const CAPTURE_CAP = 2 * 1024 * 1024; // protects the service from a runaway container

class Capped {
  private chunks: Buffer[] = [];
  private size = 0;
  push(b: Buffer) {
    if (this.size >= CAPTURE_CAP) return;
    this.chunks.push(b);
    this.size += b.length;
  }
  text() {
    return Buffer.concat(this.chunks).toString("utf8");
  }
}

/** Runs each job in a brand-new hardened container that is always removed afterwards. */
export class DockerSandbox implements Sandbox {
  constructor(
    private readonly docker: Docker,
    private readonly cfg: Config,
  ) {}

  async execute(job: SandboxJob): Promise<SandboxOutput> {
    const memory = job.limits.memoryMb * 1024 * 1024;
    const container = await this.docker.createContainer({
      Image: this.cfg.sandboxImage,
      OpenStdin: true,
      StdinOnce: true,
      AttachStdin: true,
      AttachStdout: true,
      AttachStderr: true,
      NetworkDisabled: true,
      Labels: { "zth.sandbox": "1" },
      HostConfig: {
        NetworkMode: "none",
        ReadonlyRootfs: true,
        Tmpfs: {
          "/work": "rw,exec,nosuid,size=64m,uid=10001,gid=10001,mode=0700",
          "/tmp": "rw,noexec,nosuid,size=64m,uid=10001,gid=10001,mode=1777",
        },
        CapDrop: ["ALL"],
        SecurityOpt: ["no-new-privileges"],
        PidsLimit: 128,
        Memory: memory,
        MemorySwap: memory, // no swap
        NanoCpus: 1_000_000_000,
        Ulimits: [{ Name: "nofile", Soft: 256, Hard: 256 }],
        ...(this.cfg.sandboxRuntime ? { Runtime: this.cfg.sandboxRuntime } : {}),
      },
    });

    // Outer deadline: compile + run + slack. Python enforces the precise limits inside.
    const deadlineMs = this.cfg.compileMs + job.limits.wallMs + 5000;
    let killedByDeadline = false;
    const timer = setTimeout(() => {
      killedByDeadline = true;
      container.kill().catch(() => {});
    }, deadlineMs);

    try {
      // The attach stream is used ONLY to deliver the job on stdin. Output is read from the
      // container logs after exit: attach streams can drop output from fast-exiting containers.
      const stream = await container.attach({ stream: true, stdin: true, hijack: true });
      stream.on("error", () => {});
      await container.start();
      // One JSON line. Do NOT end() the stream: a hijacked docker socket cannot half-close.
      stream.write("@@JOB " + JSON.stringify({ ...job, limits: { ...job.limits, compileMs: this.cfg.compileMs } }) + "\n");

      await container.wait();
      const { out, err } = demux(await container.logs({ stdout: true, stderr: true, follow: false }));

      const text = out.text().trim();
      if (killedByDeadline) {
        return { ...blank(), timedOut: true, internalError: "sandbox exceeded its outer deadline" };
      }
      try {
        return JSON.parse(text) as SandboxOutput;
      } catch {
        // The entrypoint died before printing a result (e.g. OOM-killed): report, don't crash.
        return { ...blank(), internalError: `sandbox produced no result. stderr: ${err.text().slice(0, 500)}` };
      }
    } finally {
      clearTimeout(timer);
      await container.remove({ force: true }).catch(() => {});
    }
  }
}

function blank(): SandboxOutput {
  return {
    compiled: false,
    compileOutput: "",
    stdout: "",
    stderr: "",
    exitCode: null,
    signal: null,
    timedOut: false,
    compileMs: 0,
    runMs: 0,
  };
}

/** Split Docker's multiplexed log buffer (8-byte frame headers) into stdout and stderr. */
function demux(buf: Buffer): { out: Capped; err: Capped } {
  const out = new Capped();
  const err = new Capped();
  let i = 0;
  while (i + 8 <= buf.length) {
    const kind = buf[i]; // 1 = stdout, 2 = stderr
    const len = buf.readUInt32BE(i + 4);
    const frame = buf.subarray(i + 8, i + 8 + len);
    (kind === 2 ? err : out).push(frame);
    i += 8 + len;
  }
  return { out, err };
}
