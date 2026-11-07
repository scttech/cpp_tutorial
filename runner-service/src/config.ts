function int(name: string, fallback: number): number {
  const raw = process.env[name];
  if (raw === undefined || raw === "") return fallback;
  const n = Number(raw);
  if (!Number.isInteger(n) || n < 0) throw new Error(`${name} must be a non-negative integer`);
  return n;
}

export const config = {
  port: int("PORT", 8080),
  host: process.env.HOST ?? "0.0.0.0",
  sandboxImage: process.env.SANDBOX_IMAGE ?? "zth-sandbox",
  /** e.g. "runsc" to run sandboxes under gVisor. Empty = Docker default (runc). */
  sandboxRuntime: process.env.SANDBOX_RUNTIME ?? "",
  maxConcurrent: int("MAX_CONCURRENT", 4),
  maxQueued: int("MAX_QUEUED", 16),
  rateLimitPerMinute: int("RATE_LIMIT_PER_MINUTE", 60),
  corsOrigin: process.env.CORS_ORIGIN ?? "http://localhost:5173",
  compileMs: int("COMPILE_MS", 30_000),
  /** Hard ceilings; requests asking for more are clamped. */
  ceilings: {
    wallMs: int("MAX_WALL_MS", 10_000),
    memoryMb: int("MAX_MEMORY_MB", 512),
    maxOutputBytes: int("MAX_OUTPUT_BYTES", 262_144),
  },
};

export type Config = typeof config;
