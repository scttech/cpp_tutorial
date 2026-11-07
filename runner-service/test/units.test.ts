import { describe, expect, it } from "vitest";
import { config } from "../src/config.js";
import { parseDiagnostics } from "../src/diagnostics.js";
import { JobQueue, QueueFullError } from "../src/queue.js";
import { buildServer } from "../src/server.js";
import type { Sandbox, SandboxOutput } from "../src/types.js";
import { normalize, runBodySchema } from "../src/validate.js";
import { extractTestResults } from "../src/zth.js";

describe("parseDiagnostics", () => {
  it("parses errors, warnings and notes, stripping /work/", () => {
    const raw = [
      "/work/main.cpp:3:5: error: use of undeclared identifier 'x'",
      "    3 |     x = 1;",
      "      |     ^",
      "main.cpp:7:1: warning: unused variable 'y' [-Wunused-variable]",
      "main.cpp:2:1: note: declared here",
      "main.cpp:1:10: fatal error: 'nope.h' file not found",
    ].join("\n");
    const d = parseDiagnostics(raw);
    expect(d).toHaveLength(4);
    expect(d[0]).toMatchObject({ severity: "error", file: "main.cpp", line: 3, column: 5 });
    expect(d[1]?.severity).toBe("warning");
    expect(d[2]?.severity).toBe("note");
    expect(d[3]?.severity).toBe("error");
  });
});

describe("parseDiagnostics: linker output", () => {
  it("parses undefined reference, multiple definition and the final link failure", () => {
    const raw = [
      "/usr/bin/ld: /tmp/main-7f4a4e.o: in function `main':",
      "main.cpp:(.text+0x15): undefined reference to `f(int)'",
      "/usr/bin/ld: /tmp/b-a23990.o: in function `g()':",
      "b.cpp:(.text+0x0): multiple definition of `g()'; /tmp/a-8161fb.o:a.cpp:(.text+0x0): first defined here",
      "clang++: error: linker command failed with exit code 1 (use -v to see invocation)",
    ].join("\n");
    const d = parseDiagnostics(raw);
    expect(d).toEqual([
      { severity: "error", file: "main.cpp", line: 0, column: 0, message: "undefined reference to `f(int)'" },
      {
        severity: "error",
        file: "b.cpp",
        line: 0,
        column: 0,
        message: "multiple definition of `g()'; /tmp/a-8161fb.o:a.cpp:(.text+0x0): first defined here",
      },
      { severity: "error", file: "(linker)", line: 0, column: 0, message: "linker command failed with exit code 1 (use -v to see invocation)" },
    ]);
  });
});

describe("extractTestResults", () => {
  it("separates protocol lines from user output", () => {
    const out = [
      "hello",
      '@@ZTH {"event":"test","name":"a","tier":"works","status":"pass","ms":0.3}',
      '@@ZTH {"event":"test","name":"b","tier":"robust","status":"fail","msg":"expected 1, got 2"}',
      "@@ZTH not json",
      '@@ZTH {"event":"summary","passed":1,"failed":1}',
      "world",
    ].join("\n");
    const r = extractTestResults(out);
    expect(r.stdout).toBe("hello\nworld");
    expect(r.tests).toEqual([
      { name: "a", status: "pass", tier: "works", durationMs: 0.3 },
      { name: "b", status: "fail", tier: "robust", message: "expected 1, got 2" },
    ]);
  });
});

describe("validation", () => {
  const ok = { userFiles: { "main.cpp": "int main(){}" } };
  it("accepts a minimal request and applies defaults", () => {
    const n = normalize(runBodySchema.parse({ ...ok, flags: ["-std=c++20", "-Wall"] }), config);
    expect(n.limits.wallMs).toBe(5000);
    expect(n.mode).toBe("run");
  });
  it("clamps limits to server ceilings", () => {
    const n = normalize(runBodySchema.parse({ ...ok, limits: { wallMs: 999999, memoryMb: 99999 } }), config);
    expect(n.limits.wallMs).toBe(config.ceilings.wallMs);
    expect(n.limits.memoryMb).toBe(config.ceilings.memoryMb);
  });
  it.each([["-fplugin=/x.so"], ["@/etc/passwd"], ["-o/etc/x"], ["--sysroot=/"], ["-I/etc"], ["-std=c++20;ls"]])(
    "rejects dangerous flag %s",
    (flag) => {
      expect(runBodySchema.safeParse({ ...ok, flags: [flag] }).success).toBe(false);
    },
  );
  it.each([["../evil.cpp"], ["/abs.cpp"], ["a b.cpp"], [""]])("rejects file name %j", (name) => {
    expect(runBodySchema.safeParse({ userFiles: { [name]: "x" } }).success).toBe(false);
  });
});

describe("JobQueue", () => {
  it("limits concurrency and rejects when the wait queue is full", async () => {
    const q = new JobQueue(1, 1);
    let release!: () => void;
    const gate = new Promise<void>((r) => (release = r));
    const first = q.run(() => gate);
    const second = q.run(async () => "second");
    await expect(q.run(async () => "third")).rejects.toBeInstanceOf(QueueFullError);
    expect(q.stats).toEqual({ running: 1, queued: 1 });
    release();
    await first;
    expect(await second).toBe("second");
    expect(q.stats).toEqual({ running: 0, queued: 0 });
  });
});

describe("POST /run (fake sandbox)", () => {
  const fake: Sandbox = {
    async execute(): Promise<SandboxOutput> {
      return {
        compiled: true,
        compileOutput: "main.cpp:1:1: warning: w",
        stdout: 'hi\n@@ZTH {"event":"test","name":"t","status":"pass"}',
        stderr: "",
        exitCode: 0,
        signal: null,
        timedOut: false,
        compileMs: 5,
        runMs: 1,
      };
    },
  };
  it("returns a RunResult with diagnostics and parsed tests", async () => {
    const app = await buildServer({ sandbox: fake, config, logger: false });
    const res = await app.inject({
      method: "POST",
      url: "/run",
      payload: { userFiles: { "main.cpp": "int main(){}" }, flags: ["-std=c++20"] },
    });
    expect(res.statusCode).toBe(200);
    const body = res.json();
    expect(body.stdout).toBe("hi");
    expect(body.tests).toEqual([{ name: "t", status: "pass" }]);
    expect(body.diagnostics[0].severity).toBe("warning");
    await app.close();
  });
  it("400s on a bad request", async () => {
    const app = await buildServer({ sandbox: fake, config, logger: false });
    const res = await app.inject({ method: "POST", url: "/run", payload: { userFiles: { "../x.cpp": "" } } });
    expect(res.statusCode).toBe(400);
    await app.close();
  });
});
