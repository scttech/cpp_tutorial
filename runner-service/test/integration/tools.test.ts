// Requires Docker and the `zth-sandbox` image. Covers what the Testing and Tooling module needs from the sandbox: linking test
// frameworks (`libs`), grading a learner's tests against buggy variants (`mode: "grade"`), and the analyzers (`analyzers`).
import Docker from "dockerode";
import { afterAll, beforeAll, describe, expect, it, vi } from "vitest";
import { config } from "../../src/config.js";
import { DockerSandbox } from "../../src/sandbox.js";
import { buildServer } from "../../src/server.js";

// Compiling a GoogleTest file takes a couple of seconds, and grading builds it once per variant.
vi.setConfig({ testTimeout: 60_000 });

let app: Awaited<ReturnType<typeof buildServer>>;
const docker = new Docker();

beforeAll(async () => {
  app = await buildServer({ sandbox: new DockerSandbox(docker, config), config, logger: false });
});
afterAll(async () => {
  await app.close();
});

async function post(payload: Record<string, unknown>) {
  const res = await app.inject({ method: "POST", url: "/run", payload: { limits: { wallMs: 6000, memoryMb: 256 }, flags: ["-std=c++20", "-Wall", "-Wextra"], ...payload } });
  expect(res.statusCode, res.body).toBe(200);
  return res.json();
}

const STACK = `#pragma once
#include <stdexcept>
#include <vector>
class Stack {
public:
    void push(int x) { v_.push_back(x); }
    int pop() {
        if (v_.empty()) throw std::out_of_range("empty");
        int x = v_.back();
        v_.pop_back();
        return x;
    }
    bool empty() const { return v_.empty(); }
    std::size_t size() const { return v_.size(); }
private:
    std::vector<int> v_;
};
`;

const GTEST = `#include <gtest/gtest.h>
#include "stack.hpp"
TEST(Stack, PopReturnsLastPushed) { Stack s; s.push(1); s.push(2); EXPECT_EQ(s.pop(), 2); EXPECT_EQ(s.pop(), 1); }
TEST(Stack, PopOnEmptyThrows) { Stack s; EXPECT_THROW(s.pop(), std::out_of_range); }
TEST(Stack, SizeTracksPushes) { Stack s; s.push(7); EXPECT_EQ(s.size(), 1u); EXPECT_FALSE(s.empty()); }
`;

describe("libs", () => {
  it("links GoogleTest", async () => {
    const r = await post({ mode: "run", userFiles: { "stack.hpp": STACK, "stack_test.cpp": GTEST }, libs: ["gtest"] });
    expect(r.compiled, r.compileOutput).toBe(true);
    expect(r.stdout).toContain("[  PASSED  ] 3 tests.");
  });
  it("links GoogleMock", async () => {
    const r = await post({
      mode: "run",
      libs: ["gmock"],
      userFiles: {
        "m.cpp": `#include <gmock/gmock.h>
struct Sink { virtual ~Sink() = default; virtual void write(int) = 0; };
struct MockSink : Sink { MOCK_METHOD(void, write, (int), (override)); };
TEST(M, Calls) { MockSink m; EXPECT_CALL(m, write(testing::Gt(1))).Times(2); m.write(2); m.write(3); }
`,
      },
    });
    expect(r.stdout, r.compileOutput).toContain("[  PASSED  ] 1 test.");
  });
  it("links Catch2", async () => {
    const r = await post({
      mode: "run",
      libs: ["catch2"],
      userFiles: { "c.cpp": `#include <catch2/catch_test_macros.hpp>\nTEST_CASE("math") { REQUIRE(1 + 1 == 2); }\n` },
    });
    expect(r.stdout, r.compileOutput).toContain("All tests passed");
  });
  it("rejects an unknown library", async () => {
    const res = await app.inject({ method: "POST", url: "/run", payload: { userFiles: { "a.cpp": "int main(){}" }, libs: ["boost"] } });
    expect(res.statusCode).toBe(400);
  });
});

describe("grade mode", () => {
  const grade = {
    variants: [
      { name: "pop-returns-first", label: "pop() returns the oldest item", hint: "Push two items and check which one comes out." },
      { name: "pop-empty-no-throw", label: "pop() on an empty stack does not throw", hint: "Test the empty case." },
    ],
  };
  const tests = (extra: Record<string, string> = {}) => ({
    "grade.json": JSON.stringify(grade),
    "variants/pop-returns-first/stack.hpp": STACK.replace("int x = v_.back();\n        v_.pop_back();", "int x = v_.front();\n        v_.erase(v_.begin());"),
    "variants/pop-empty-no-throw/stack.hpp": STACK.replace('if (v_.empty()) throw std::out_of_range("empty");', "if (v_.empty()) return 0;"),
    ...extra,
  });
  const byName = (r: { tests: { name: string; status: string; message?: string }[] }) => Object.fromEntries(r.tests.map((t) => [t.name, t]));

  it("passes tests that catch every variant", async () => {
    const r = await post({ mode: "grade", libs: ["gtest"], userFiles: { "stack.hpp": STACK, "stack_test.cpp": GTEST }, testFiles: tests() });
    expect(r.compiled, r.compileOutput).toBe(true);
    expect(r.tests.map((t: { status: string }) => t.status), JSON.stringify(r.tests)).toEqual(["pass", "pass", "pass"]);
    expect(r.stdout).toContain("[  PASSED  ] 3 tests."); // the learner still sees their own framework's output
  });

  it("names the bug that the tests miss", async () => {
    const weak = `#include <gtest/gtest.h>\n#include "stack.hpp"\nTEST(Stack, SizeTracksPushes) { Stack s; s.push(7); EXPECT_EQ(s.size(), 1u); }\n`;
    const r = await post({ mode: "grade", libs: ["gtest"], userFiles: { "stack.hpp": STACK, "stack_test.cpp": weak }, testFiles: tests() });
    const t = byName(r);
    expect(t["Your tests pass on the correct code"]?.status).toBe("pass");
    expect(t["pop() returns the oldest item"]?.status).toBe("fail"); // not caught: a single push cannot tell first from last
    expect(t["pop() returns the oldest item"]?.message).toContain("Push two items");
    expect(t["pop() on an empty stack does not throw"]?.status).toBe("fail");
    expect(t["pop() on an empty stack does not throw"]?.message).toContain("Test the empty case.");
  });

  it("does not check variants when the tests fail on correct code", async () => {
    const broken = `#include <gtest/gtest.h>\n#include "stack.hpp"\nTEST(Stack, Wrong) { Stack s; EXPECT_EQ(s.size(), 5u); }\n`;
    const r = await post({ mode: "grade", libs: ["gtest"], userFiles: { "stack.hpp": STACK, "stack_test.cpp": broken }, testFiles: tests() });
    expect(r.tests.map((t: { status: string }) => t.status)).toEqual(["fail", "fail", "fail"]);
  });

  it("reports a compile error in the learner's tests like any other build", async () => {
    const r = await post({ mode: "grade", libs: ["gtest"], userFiles: { "stack.hpp": STACK, "stack_test.cpp": "TEST(" }, testFiles: tests() });
    expect(r.compiled).toBe(false);
    expect(r.diagnostics.length).toBeGreaterThan(0);
  });

  it("works with Catch2", async () => {
    const c = `#include <catch2/catch_test_macros.hpp>\n#include "stack.hpp"\nTEST_CASE("order") { Stack s; s.push(1); s.push(2); REQUIRE(s.pop() == 2); }\nTEST_CASE("empty") { Stack s; REQUIRE_THROWS_AS(s.pop(), std::out_of_range); }\n`;
    const r = await post({ mode: "grade", libs: ["catch2"], userFiles: { "stack.hpp": STACK, "stack_test.cpp": c }, testFiles: tests() });
    expect(r.tests.map((t: { status: string }) => t.status), JSON.stringify(r.tests)).toEqual(["pass", "pass", "pass"]);
  });
});

describe("analyzers", () => {
  const messy = `int f(int x) {
    int unused;
    if (x > 0)
        return 1;
    return 0;
}
int main() { return f(1); }
`;
  const run = (analyzers: unknown[], code = messy, extra: Record<string, string> = {}) =>
    post({
      mode: "test",
      userFiles: { "a.cpp": code, ...extra },
      testFiles: { "t.cpp": '#include <cstdio>\nextern "C" int zth_user_main();\nint main() { std::printf("@@ZTH {\\"event\\":\\"test\\",\\"name\\":\\"base\\",\\"tier\\":\\"works\\",\\"status\\":\\"pass\\"}\\n"); }\n' },
      analyzers,
    });
  const find = (r: { tests: { name: string; status: string; message?: string }[] }, name: string) => r.tests.find((t) => t.name === name);

  it("clang-tidy reports findings in the learner's file and turns them into Problems", async () => {
    const r = await run([{ tool: "clang-tidy", files: ["a.cpp"], checks: "-*,readability-braces-around-statements" }]);
    const t = find(r, "clang-tidy: no findings");
    expect(t, JSON.stringify(r.tests)).toMatchObject({ status: "fail" });
    expect(t?.message).toContain("a.cpp:3");
    expect(r.diagnostics.some((d: { file: string; line: number }) => d.file === "a.cpp" && d.line === 3)).toBe(true);
  });
  it("clang-tidy passes a file with nothing to report", async () => {
    const r = await run([{ tool: "clang-tidy", files: ["a.cpp"], checks: "-*,readability-braces-around-statements" }], "int main() { return 0; }\n");
    expect(find(r, "clang-tidy: no findings")?.status).toBe("pass");
  });
  it("clang-tidy reads a config file the learner can edit", async () => {
    const r = await run([{ tool: "clang-tidy", files: ["a.cpp"], config: "tidy.yaml" }], messy, { "tidy.yaml": "Checks: '-*,readability-braces-around-statements'\n" });
    expect(find(r, "clang-tidy: no findings")?.status, JSON.stringify(r.tests)).toBe("fail");
  });
  it("cppcheck reports findings", async () => {
    const r = await run([{ tool: "cppcheck", files: ["a.cpp"] }], "int main() { int x; return x; }\n");
    expect(find(r, "cppcheck: no findings"), JSON.stringify(r.tests)).toMatchObject({ status: "fail" });
  });
  it("clang-format checks layout", async () => {
    const bad = await run([{ tool: "clang-format", files: ["a.cpp"] }], "int main(){return 0;}\n");
    expect(find(bad, "clang-format: no findings")?.status, JSON.stringify(bad.tests)).toBe("fail");
    const good = await run([{ tool: "clang-format", files: ["a.cpp"] }], "int main() {\n  int a = 1;\n  return a;\n}\n"); // (LLVM style would join a body of one statement onto one line)
    expect(find(good, "clang-format: no findings")?.status, JSON.stringify(good.tests)).toBe("pass");
  });
  it("rejects shell metacharacters in checks", async () => {
    const res = await app.inject({ method: "POST", url: "/run", payload: { mode: "test", userFiles: { "a.cpp": "int main(){}" }, analyzers: [{ tool: "clang-tidy", files: ["a.cpp"], checks: "x;rm -rf /" }] } });
    expect(res.statusCode).toBe(400);
  });
});
