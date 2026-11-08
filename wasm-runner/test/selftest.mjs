// Self-test of the WASM runner on small programs. usage: node test/selftest.mjs
import { createNodeRunner } from "../src/node-host.mjs";

let failures = 0;
const check = (ok, what, extra = "") => {
  console.log(`${ok ? "PASS" : "FAIL"}  ${what}${ok ? "" : "   " + extra}`);
  if (!ok) failures++;
};
const runner = createNodeRunner({ workers: 3 });

try {
  // ---- run mode ----
  let r = await runner.run({
    mode: "run", flags: [],
    userFiles: { "main.cpp": '#include <iostream>\n#include <string>\nint main(){ std::string n; std::cin >> n; std::cout << "Hello, " << n << "!\\n"; }\n' },
    stdin: "wasm\n",
  });
  check(r.compiled && r.stdout === "Hello, wasm!\n" && r.exitCode === 0, "run: stdin to stdout", JSON.stringify(r).slice(0, 300));

  r = await runner.run({ mode: "run", flags: [], userFiles: { "main.cpp": "int main(){ int x = \"s\"; }\n" } });
  check(!r.compiled && /error:/.test(r.compileOutput) && /main\.cpp:1:/.test(r.compileOutput), "run: a compile error is reported with file and line", r.compileOutput.slice(0, 200));

  r = await runner.run({ mode: "run", flags: [], userFiles: { "main.cpp": "int main(){ for(;;){} }\n" }, limits: { wallMs: 1500 } });
  check(r.compiled && r.timedOut && r.exitCode === null, "run: an infinite loop is stopped by the time limit");

  r = await runner.run({ mode: "run", flags: [], userFiles: { "main.cpp": "#include <iostream>\nint main(){ for(;;) std::cout << \"x\"; }\n" }, limits: { maxOutputBytes: 1000 } });
  check(r.compiled && r.stdout.includes("[output truncated]") && !r.timedOut, "run: runaway output is cut off, not timed out", `${r.stdout.length} bytes, timedOut=${r.timedOut}`);

  r = await runner.run({ mode: "run", flags: [], userFiles: { "main.cpp": "#include <cstdlib>\nint main(){ std::abort(); }\n" } });
  check(r.compiled && r.exitCode === null && r.signal === "SIGABRT" && /crashed/.test(r.stderr), "run: abort() is reported as a crash", JSON.stringify(r).slice(0, 300));

  r = await runner.run({ mode: "run", flags: [], userFiles: { "main.cpp": "#include <stdexcept>\nint main(){ try { throw 1; } catch (int) { return 0; } }\n" } });
  check(!r.compiled && /exception/i.test(r.compileOutput), "run: try/catch does not compile (that is what greys such lessons out)", r.compileOutput.slice(0, 200));

  // ---- test mode: several files, renamed main, source reading, per-test isolation ----
  const tests = `#include "zth_test.hpp"
#include <cstdlib>
int add(int, int);
ZTH_TEST("works", "adds") { ZTH_EXPECT_EQ(add(2, 3), 5); }
ZTH_TEST("works", "fails with a message") { ZTH_EXPECT_EQ(add(2, 2), 5); }
ZTH_TEST("robust", "an infinite loop times out") { for (;;) {} }
ZTH_TEST("robust", "abort is an error, the next test still runs") { std::abort(); }
ZTH_TEST("works", "can read the learner's source") { ZTH_EXPECT_CONTAINS(zth::read_file("main.cpp"), "int add"); }
ZTH_TEST("works", "runs the learner's main with stdin") {
    auto r = ZTH_RUN_USER_MAIN("hi\\n");
    ZTH_EXPECT_EQ(r.out, std::string("echo: hi\\n"));
    ZTH_EXPECT_EQ(r.exit_code, 0);  // main has no return statement: it must still return 0
}
`;
  const main = '#include <iostream>\n#include <string>\nint add(int a, int b);\nint main(){ std::string s; std::cin >> s; std::cout << "echo: " << s << "\\n"; }\n';
  const lib = "int add(int a, int b) { return a + b; }\n";
  const files = { "main.cpp": main.replace("int add(int a, int b);", "int add(int a, int b);"), "add.cpp": lib };
  const t0 = Date.now();
  r = await runner.run({ mode: "test", flags: [], userFiles: files, testFiles: { "test.cpp": tests } });
  const status = Object.fromEntries(r.tests.map((t) => [t.name, t]));
  check(r.compiled, "test: a two-file program with hidden tests builds", r.compileOutput.slice(0, 300));
  check(status["adds"]?.status === "pass" && status["adds"]?.tier === "works", "test: a passing test");
  check(status["fails with a message"]?.status === "fail" && /expected 5, got 4/.test(status["fails with a message"]?.message ?? ""), "test: a failing test carries its message", JSON.stringify(status["fails with a message"]));
  check(status["an infinite loop times out"]?.status === "timeout", "test: an infinite loop is a timeout", JSON.stringify(status["an infinite loop times out"]));
  check(status["abort is an error, the next test still runs"]?.status === "error" && /abort/.test(status["abort is an error, the next test still runs"]?.message ?? ""), "test: abort is an error", JSON.stringify(status["abort is an error, the next test still runs"]));
  check(status["can read the learner's source"]?.status === "pass", "test: tests can read the learner's files", JSON.stringify(status["can read the learner's source"]));
  check(status["runs the learner's main with stdin"]?.status === "pass", "test: ZTH_RUN_USER_MAIN feeds stdin and main returns 0 without a return statement", JSON.stringify(status["runs the learner's main with stdin"]));
  check(r.tests.length === 6 && r.exitCode === 1, "test: all six tests are reported", `${r.tests.length} tests`);
  console.log(`      (${Date.now() - t0} ms)`);

  // the hidden tests are compiled once: a second attempt reuses the object
  const t1 = Date.now();
  const r2 = await runner.run({ mode: "test", flags: [], userFiles: { ...files, "add.cpp": "int add(int a, int b) { return a - b; }\n" }, testFiles: { "test.cpp": tests } });
  check(r2.compiled && r2.tests.find((t) => t.name === "adds")?.status === "fail", "test: a different attempt is judged separately");
  console.log(`      (${Date.now() - t1} ms, hidden tests came from the cache)`);

  r = await runner.run({ mode: "test", flags: [], userFiles: { "main.cpp": "int add(int a, int b){ return a +; }\n" }, testFiles: { "test.cpp": tests } });
  check(!r.compiled && /error:/.test(r.compileOutput) && !/test\.cpp/.test(r.compileOutput), "test: the learner's error is reported first, without noise from the tests");
  // ---- linker errors look like the Docker runner's, so the friendly-error rules apply to both ----
  r = await runner.run({ mode: "run", flags: [], userFiles: { "main.cpp": "int f(int);\nint main(){ return f(1); }\n" } });
  check(!r.compiled && r.diagnostics.some((d) => /^undefined reference to `f\(int\)'$/.test(d.message) && d.file === "main.cpp"), "link: an undefined function reads like GNU ld", r.compileOutput);
  check(r.diagnostics.some((d) => /linker command failed/.test(d.message)), "link: and the generic 'linker command failed' line follows");

  r = await runner.run({ mode: "run", flags: [], userFiles: { "a.cpp": "int f(){return 1;}\nint main(){return f();}\n", "b.cpp": "int f(){return 2;}\n" } });
  check(!r.compiled && r.diagnostics.some((d) => /^multiple definition of `f\(\)'; /.test(d.message)), "link: a duplicate definition reads like GNU ld", r.compileOutput);

  r = await runner.run({ mode: "run", flags: [], userFiles: { "main.cpp": "int main(){ int x = 1 }\n" } });
  check(r.diagnostics[0]?.severity === "error" && r.diagnostics[0]?.line === 1 && r.diagnostics[0]?.file === "main.cpp", "compile: diagnostics are parsed (file, line, severity)", JSON.stringify(r.diagnostics[0]));
} catch (e) {
  failures++;
  console.log("FAIL  unexpected error:", e.stack);
} finally {
  await runner.close();
}
console.log(failures ? `\n${failures} check(s) failed` : "\nwasm runner self-test passed");
process.exit(failures ? 1 : 0);
