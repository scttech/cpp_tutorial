// Requires Docker and the `zth-sandbox` image (docker build -t zth-sandbox ../runner-image).
// These are the "attack" tests: hostile programs must be contained and reported, not crash the service.
import Docker from "dockerode";
import { afterAll, beforeAll, describe, expect, it } from "vitest";
import { config } from "../../src/config.js";
import { DockerSandbox } from "../../src/sandbox.js";
import { buildServer } from "../../src/server.js";

let app: Awaited<ReturnType<typeof buildServer>>;
const docker = new Docker();

async function run(source: string, opts: { flags?: string[]; wallMs?: number; stdin?: string; extra?: Record<string, string>; mode?: string } = {}) {
  const res = await app.inject({
    method: "POST",
    url: "/run",
    payload: {
      mode: opts.mode ?? "run",
      userFiles: { "main.cpp": source, ...(opts.extra ?? {}) },
      flags: opts.flags ?? ["-std=c++20", "-pthread"],
      stdin: opts.stdin,
      limits: { wallMs: opts.wallMs ?? 3000, memoryMb: 128 },
    },
  });
  expect(res.statusCode).toBe(200);
  return res.json();
}

async function leftoverSandboxes() {
  return (await docker.listContainers({ all: true, filters: { label: ["zth.sandbox=1"] } })).length;
}

beforeAll(async () => {
  app = await buildServer({ sandbox: new DockerSandbox(docker, config), config, logger: false });
});
afterAll(async () => {
  await app.close();
});

describe("functional", () => {
  it("compiles, runs, and passes stdin through", async () => {
    const r = await run(
      `#include <iostream>
#include <string>
int main(){ std::string s; std::cin >> s; std::cout << "hi " << s; }`,
      { stdin: "bob" },
    );
    expect(r).toMatchObject({ compiled: true, stdout: "hi bob", exitCode: 0, timedOut: false });
  });
  it("supports exceptions", async () => {
    const r = await run(`#include <iostream>
#include <stdexcept>
int main(){ try { throw std::runtime_error("boom"); } catch (const std::exception& e) { std::cout << e.what(); } }`);
    expect(r.stdout).toBe("boom");
  });
  it("returns structured diagnostics for compile errors", async () => {
    const r = await run(`int main(){ int x = "s"; }`);
    expect(r.compiled).toBe(false);
    expect(r.diagnostics.some((d: { severity: string; line: number }) => d.severity === "error" && d.line === 1)).toBe(true);
  });
  it("reports a crash signal", async () => {
    const r = await run(`#include <csignal>
int main(){ std::raise(SIGSEGV); }`);
    expect(r.signal).toBe("SIGSEGV");
  });
  it("test mode compiles hidden tests with user code", async () => {
    const res = await app.inject({
      method: "POST",
      url: "/run",
      payload: {
        mode: "test",
        userFiles: { "solution.hpp": "inline int add(int a,int b){return a+b;}" },
        testFiles: {
          "test.cpp": `#include <cstdio>
#include "solution.hpp"
int main(){ std::printf("@@ZTH {\\"event\\":\\"test\\",\\"name\\":\\"add\\",\\"tier\\":\\"works\\",\\"status\\":\\"%s\\"}\\n", add(2,3)==5?"pass":"fail"); }`,
        },
        flags: ["-std=c++20"],
      },
    });
    expect(res.json().tests).toEqual([{ name: "add", status: "pass", tier: "works" }]);
  });
});

describe("containment", () => {
  it("kills an infinite loop", async () => {
    const r = await run(`int main(){ for(;;){} }`, { wallMs: 1000 });
    expect(r.timedOut).toBe(true);
  });
  it("blocks the network", async () => {
    const r = await run(`#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
int main(){ int s=socket(AF_INET,SOCK_STREAM,0); sockaddr_in a{}; a.sin_family=AF_INET; a.sin_port=htons(80); inet_pton(AF_INET,"1.1.1.1",&a.sin_addr); return connect(s,(sockaddr*)&a,sizeof a)==0 ? 0 : 7; }`);
    expect(r, JSON.stringify(r)).toMatchObject({ exitCode: 7 });
  });
  it("contains a fork bomb (pids limit) and the service survives", async () => {
    const r = await run(`#include <unistd.h>
int main(){ for(;;) fork(); }`, { wallMs: 2000 });
    expect(r.timedOut || r.signal !== null || r.exitCode !== 0).toBe(true);
    expect((await app.inject({ method: "GET", url: "/healthz" })).statusCode).toBe(200);
  }, 30_000);
  it("contains a memory hog (cgroup limit)", async () => {
    const r = await run(`#include <cstdlib>
#include <cstring>
int main(){ for(;;){ char* p=(char*)std::malloc(1<<20); if(p) std::memset(p,1,1<<20); } }`, { wallMs: 5000 });
    expect(r.exitCode === 0 && !r.timedOut).toBe(false);
  }, 30_000);
  it("caps huge output", async () => {
    const r = await run(`#include <cstdio>
int main(){ for(int i=0;i<2000000;i++) std::puts("xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx"); }`, { wallMs: 5000 });
    expect(r.stdout.length).toBeLessThan(70_000);
    expect(r.stdout).toContain("[output truncated]");
  }, 30_000);
  it("cannot write outside the tmpfs (read-only rootfs)", async () => {
    const r = await run(`#include <cstdio>
int main(){ FILE* f = std::fopen("/etc/pwned","w"); std::puts(f ? "WROTE" : "denied"); }`);
    expect(r.stdout.trim(), JSON.stringify(r)).toBe("denied");
  });
  it("runs as an unprivileged user", async () => {
    const r = await run(`#include <unistd.h>
#include <cstdio>
int main(){ std::printf("%d", (int)getuid()); }`);
    expect(r.stdout).toBe("10001");
  });
  it("leaves no containers behind", async () => {
    expect(await leftoverSandboxes()).toBe(0);
  });
});
