import cors from "@fastify/cors";
import rateLimit from "@fastify/rate-limit";
import Fastify from "fastify";
import type { Config } from "./config.js";
import { parseDiagnostics } from "./diagnostics.js";
import { parseRuntimeReport } from "./runtimeReport.js";
import { JobQueue, QueueFullError } from "./queue.js";
import type { RunResult, Sandbox } from "./types.js";
import { normalize, runBodySchema } from "./validate.js";
import { extractTestResults } from "./zth.js";

export async function buildServer(deps: { sandbox: Sandbox; config: Config; logger?: boolean }) {
  const { sandbox, config } = deps;
  const app = Fastify({
    logger: deps.logger ?? true,
    bodyLimit: 1024 * 1024,
  });
  const queue = new JobQueue(config.maxConcurrent, config.maxQueued);

  await app.register(cors, { origin: config.corsOrigin });
  await app.register(rateLimit, { max: config.rateLimitPerMinute, timeWindow: "1 minute" });

  app.get("/healthz", { config: { rateLimit: false } }, async () => ({ ok: true, ...queue.stats }));

  app.post("/run", async (req, reply) => {
    const parsed = runBodySchema.safeParse(req.body);
    if (!parsed.success) {
      return reply.code(400).send({
        error: "invalid request",
        issues: parsed.error.issues.map((i) => `${i.path.join(".") || "(body)"}: ${i.message}`),
      });
    }
    const job = normalize(parsed.data, config);

    let raw;
    try {
      raw = await queue.run(() =>
        sandbox.execute({
          files: job.userFiles,
          testFiles: job.testFiles,
          flags: job.flags,
          libs: job.libs,
          analyzers: job.analyzers,
          stdin: job.stdin ?? "",
          mode: job.mode,
          limits: job.limits,
        }),
      );
    } catch (e) {
      if (e instanceof QueueFullError) return reply.code(503).header("Retry-After", "2").send({ error: e.message });
      req.log.error(e, "sandbox failure");
      return reply.code(500).send({ error: "sandbox failure" });
    }

    if (raw.internalError) req.log.error({ internalError: raw.internalError }, "sandbox internal error");
    const { stdout, tests } = extractTestResults(raw.stdout);
    const result: RunResult = {
      compiled: raw.compiled,
      diagnostics: parseDiagnostics(raw.compileOutput),
      compileOutput: raw.compileOutput,
      stdout,
      stderr: raw.stderr,
      tests,
      exitCode: raw.exitCode,
      signal: raw.signal,
      timedOut: raw.timedOut,
      timings: { compileMs: raw.compileMs, runMs: raw.runMs },
    };
    if (job.flags.some((f) => f.startsWith("-fsanitize="))) {
      const report = parseRuntimeReport(raw.stderr);
      if (report) result.report = report;
    }
    return result;
  });

  return app;
}
