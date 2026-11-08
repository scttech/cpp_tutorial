// Checks the SANITIZER support end to end against the real sandbox, with runner-image/test/san-selftest:
//   - a sanitizer error ends only ITS test (status "error", message "stopped by a sanitizer: ... <kind>"), the next
//     tests still run, and an ordinary failure is still an ordinary failure
//   - the harness's allocation tracking (ZTH_TRACK_ALLOCATIONS) still works with AddressSanitizer on
//   - the runner returns a parsed `report` for the first problem, with a location in the learner's file
//   - the same sources built WITHOUT -fsanitize produce no report
//   usage: node sanitizer-selftest.mjs      (needs the runner: docker compose up -d runner)
import { readFile } from "node:fs/promises";
import { fileURLToPath } from "node:url";
import { runJob } from "./runner-client.mjs";

const dir = new URL("../../runner-image/test/san-selftest/", import.meta.url);
const [main, test] = await Promise.all(["main.cpp", "test.cpp"].map((f) => readFile(fileURLToPath(new URL(f, dir)), "utf8")));
const job = (flags) => runJob({ mode: "test", userFiles: { "main.cpp": main }, testFiles: { "test.cpp": test }, flags, limits: { memoryMb: 256 } });

let bad = 0;
const check = (ok, what) => {
  console.log(`${ok ? "PASS" : "FAIL"}  ${what}`);
  if (!ok) bad++;
};

const san = await job(["-std=c++20", "-Wall", "-Wextra", "-fsanitize=address,undefined"]);
check(san.compiled, `compiles with -fsanitize=address,undefined${san.compiled ? "" : "\n" + san.compileOutput.slice(0, 400)}`);
check(san.tests?.length === 8, `all 8 tests reported a result (got ${san.tests?.length})`);
for (const t of san.tests ?? []) {
  const err = /^expect-error (.+?):/.exec(t.name);
  let ok, shown = "";
  if (err) {
    ok = t.status === "error" && t.message?.startsWith("stopped by a sanitizer: ") && t.message.replace(/-/g, " ").includes(err[1].replace(/-/g, " "));
    shown = ` -> ${t.status}: ${t.message}`;
  } else if (t.name.startsWith("expect-fail")) {
    ok = t.status === "fail" && /leak/i.test(t.message ?? "");
    shown = ` -> ${t.status}: ${t.message}`;
  } else {
    ok = t.status === "pass" || t.name.includes("plain wrong answer") && t.status === "fail";
    shown = ok ? "" : ` -> ${t.status}: ${t.message}`;
  }
  check(ok, `${t.name}${shown.slice(0, 200)}`);
}
check(san.report?.kind === "heap-buffer-overflow", `the runner parsed the FIRST report (heap-buffer-overflow), got ${san.report?.kind}`);
check(san.report?.location?.file === "main.cpp" && san.report.location.line === 7, `its location is the learner's main.cpp:7, got ${JSON.stringify(san.report?.location)}`);
check(/AddressSanitizer: heap-buffer-overflow/.test(san.stderr), "the raw report is still in stderr");

const plain = await job(["-std=c++20", "-Wall", "-Wextra"]);
check(plain.compiled && !plain.report, "without -fsanitize there is no report");
check(plain.tests.some((t) => t.status === "error" && /crashed|signal/i.test(t.message ?? "")) || plain.tests.every((t) => t.status !== "error" || !/sanitizer/.test(t.message ?? "")), "and no test says it was stopped by a sanitizer");

// A run where every test passes must END cleanly: no report, exit code 0 (the harness's own bookkeeping, kept after
// deletes by ZTH_TRACK_ALLOCATIONS, must not be mistaken for a leak at exit).
const passingTests = [
  "#define ZTH_TRACK_ALLOCATIONS",
  "#include <vector>",
  '#include "zth_test.hpp"',
  'ZTH_TEST("works", "a vector grows and is freed") { std::vector<int> v; for (int i = 0; i < 100; ++i) v.push_back(i); ZTH_EXPECT_EQ(v.size(), std::size_t{100}); }',
  'ZTH_TEST("works", "new and delete are paired") { int* p = new int(3); ZTH_EXPECT_EQ(*p, 3); delete p; }',
].join("\n");
const clean = await runJob({ mode: "test", userFiles: { "main.cpp": "int main() {}\n" }, testFiles: { "test.cpp": passingTests }, flags: ["-std=c++20", "-Wall", "-Wextra", "-fsanitize=address,undefined"], limits: { memoryMb: 256 } });
check(clean.tests?.length === 2 && clean.tests.every((t) => t.status === "pass"), "a passing test run under the sanitizers passes");
check(!clean.report && clean.exitCode === 0, `and ends cleanly: no report, exit code 0 (got exit ${clean.exitCode}, report ${clean.report?.summary ?? "none"})`);

console.log(bad ? `\n${bad} sanitizer self-test(s) failed` : "\nsanitizer self-test passed");
process.exit(bad ? 1 : 0);
