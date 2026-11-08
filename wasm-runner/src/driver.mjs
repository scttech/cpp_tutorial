// Turns a RunRequest into a RunResult using an abstract `host` (a Node worker pool for the compatibility check,
// browser Workers for the app). Mirrors runner-image/zth-run.py so both runners answer the same way.
//
//   host.compile({name, files, flags, renameMain}) -> {ok, output, object}
//   host.link({objects, flags, memoryMb})           -> {ok, output, module}
//   host.exec({module, args, stdin, files, timeoutMs, maxOutputBytes}) -> {stdout, stderr, exitCode, trap, truncated, timedOut}
//   host.cache                                       a Map, used to keep compiled hidden-test objects
import { DEFAULT_FLAGS } from "./toolchain.mjs";
import { normalizeLinkerOutput, parseDiagnostics } from "./diagnostics.mjs";

const PREFIX = "@@ZTH ";
const isSource = (n) => /\.(cpp|cc|cxx)$/.test(n);
const now = () => performance.now();

function protocolEvents(stdout) {
  const events = [], visible = [];
  for (const line of stdout.split("\n")) {
    if (!line.startsWith(PREFIX)) { visible.push(line); continue; }
    try { events.push(JSON.parse(line.slice(PREFIX.length))); } catch { /* malformed protocol line: drop it */ }
  }
  return { events, visible: visible.join("\n") };
}

async function cachedObject(host, key, make) {
  if (!host.cache.has(key)) host.cache.set(key, make());
  const r = await host.cache.get(key);
  if (!r.ok) host.cache.delete(key); // do not remember failures (a retry may succeed, e.g. after an internal error)
  return r;
}

/**
 * @param host see above
 * @param {import("../../app/src/runner/types").RunRequest} req
 */
export async function runRequest(host, req) {
  // This toolchain has no C++ exception support (no unwinder): without the flag, libc++'s headers still emit
  // `throw` and the link fails. With it, a learner's own try/catch is a compile error, which is how lessons
  // that need exceptions show up as incompatible.
  const requested = req.flags?.length ? req.flags : DEFAULT_FLAGS;
  const flags = requested.includes("-fno-exceptions") ? requested : [...requested, "-fno-exceptions"];
  const limits = req.limits ?? {};
  const testMode = req.mode === "test";
  const userFiles = req.userFiles ?? {}, testFiles = testMode ? req.testFiles ?? {} : {};
  const result = {
    compiled: false, diagnostics: [], compileOutput: "", stdout: "", stderr: "", tests: [],
    exitCode: null, signal: null, timedOut: false, timings: { compileMs: 0, runMs: 0 },
  };
  const userSources = Object.keys(userFiles).filter(isSource);
  const testSources = Object.keys(testFiles).filter(isSource);
  if (!userSources.length && !testSources.length) {
    result.compileOutput = "no .cpp files to compile";
    return result;
  }

  // ---- compile (units in parallel) and link ----
  const t0 = now();
  const all = { ...userFiles, ...testFiles };
  const headersKey = JSON.stringify(Object.entries(all).filter(([n]) => !isSource(n)).sort());
  const units = [
    ...userSources.map((name) => ({ name, user: true })),
    ...testSources.map((name) => ({ name, user: false })),
  ];
  const compiled = await Promise.all(
    units.map((u) => {
      const make = () => host.compile({ name: u.name, files: all, flags, renameMain: testMode && u.user });
      // The hidden tests do not change between a learner's attempts: compile them once.
      return u.user ? make() : cachedObject(host, JSON.stringify([u.name, all[u.name], headersKey, flags]), make);
    }),
  );
  // Like the container, report diagnostics up to the first unit that failed.
  let output = "";
  for (const c of compiled) { output += c.output; if (!c.ok) break; }
  result.compileOutput = output;
  if (compiled.some((c) => !c.ok)) {
    result.diagnostics = parseDiagnostics(output);
    result.timings.compileMs = Math.round(now() - t0);
    return result;
  }
  const linked = await host.link({
    objects: compiled.map((c) => c.object), flags,
    memoryMb: limits.memoryMb ?? 128,
  });
  result.compileOutput = output + (linked.ok ? linked.output : normalizeLinkerOutput(linked.output, units.map((u) => u.name)));
  result.diagnostics = parseDiagnostics(result.compileOutput);
  result.timings.compileMs = Math.round(now() - t0);
  if (!linked.ok) return result;
  result.compiled = true;

  // ---- run ----
  const t1 = now();
  const maxOutputBytes = limits.maxOutputBytes ?? 65536;
  const wallMs = limits.wallMs ?? 5000;
  const exec = (args, timeoutMs, stdin = "") =>
    host.exec({ module: linked.module, args, stdin, files: userFiles, timeoutMs, maxOutputBytes });

  if (!testMode) {
    const r = await exec([], wallMs, req.stdin ?? "");
    Object.assign(result, {
      stdout: r.stdout, stderr: r.stderr, timedOut: r.timedOut,
      exitCode: r.trap || r.timedOut ? null : r.exitCode,
      signal: r.trap ? "SIGABRT" : r.timedOut ? "SIGKILL" : null,
    });
    if (r.trap) result.stderr += `${result.stderr && !result.stderr.endsWith("\n") ? "\n" : ""}crashed: ${r.trap}\n`;
    result.timings.runMs = Math.round(now() - t1);
    return result;
  }

  const list = protocolEvents((await exec(["--list"], wallMs)).stdout).events.filter((e) => e.event === "list");
  if (!list.length) {
    result.stderr = "the test program did not list any tests";
    return result;
  }
  let visible = "";
  for (const t of list) {
    const r = await exec(["--only", String(t.index)], t.timeout * 1000 + 500);
    const { events, visible: v } = protocolEvents(r.stdout);
    visible += v.replace(/\n$/, "");
    const ev = events.find((e) => e.event === "test");
    if (ev) {
      result.tests.push({
        name: ev.name, status: ev.status, tier: ev.tier,
        ...(typeof ev.msg === "string" ? { message: ev.msg } : {}),
        ...(typeof ev.ms === "number" ? { durationMs: ev.ms } : {}),
      });
    } else if (r.timedOut) {
      result.tests.push({ name: t.name, tier: t.tier, status: "timeout", message: `test did not finish in ${t.timeout}s (infinite loop?)` });
    } else {
      const why = r.trap ? `crashed: ${r.trap}` : `the program exited (code ${r.exitCode}) without reporting a result`;
      result.tests.push({ name: t.name, tier: t.tier, status: "error", message: why });
    }
  }
  result.stdout = visible;
  result.exitCode = result.tests.every((t) => t.status === "pass") ? 0 : 1;
  result.timings.runMs = Math.round(now() - t1);
  return result;
}
