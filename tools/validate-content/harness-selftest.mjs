// Checks the test harness itself (allocation tracking, Tracked) against runner-image/test/mem-selftest/test.cpp.
// Tests whose name starts with "expect-fail" must fail with a message; every other test must pass.
//   usage: node harness-selftest.mjs      (needs the runner: docker compose up -d runner)
import { readFile } from "node:fs/promises";
import { fileURLToPath } from "node:url";
import { runJob } from "./runner-client.mjs";

const src = await readFile(fileURLToPath(new URL("../../runner-image/test/mem-selftest/test.cpp", import.meta.url)), "utf8");
const res = await runJob({ mode: "test", userFiles: { "main.cpp": "int main() {}\n" }, testFiles: { "test.cpp": src }, flags: ["-std=c++20", "-Wall", "-Wextra"] });

let bad = 0;
if (!res.tests?.length) {
  console.log("FAIL  no tests ran:", JSON.stringify(res).slice(0, 600));
  process.exit(1);
}
for (const t of res.tests) {
  const wantFail = t.name.startsWith("expect-fail");
  const ok = wantFail ? t.status === "fail" && !!t.message : t.status === "pass";
  console.log(`${ok ? "PASS" : "FAIL"}  ${t.name}${wantFail ? `   -> ${t.message}` : ok ? "" : `   -> ${t.status}: ${t.message ?? ""}`}`);
  if (!ok) bad++;
}
const leak = res.tests.find((t) => t.name.includes("a leak is reported"));
if (!leak?.message?.includes("1 allocation(s) (100 bytes)")) {
  console.log(`FAIL  leak report must count only the learner's allocation (1 block, 100 bytes), got: ${leak?.message}`);
  bad++;
}
console.log(bad ?`\n${bad} harness self-test(s) failed` : "\nharness self-test passed");
process.exit(bad ? 1 : 0);
