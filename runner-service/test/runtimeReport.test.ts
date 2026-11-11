import { describe, expect, it } from "vitest";
import { parseRuntimeReport } from "../src/runtimeReport.js";

const lines = (...ls: string[]) => ls.join("\n");

describe("parseRuntimeReport: ThreadSanitizer", () => {
  const race = lines(
    "/usr/bin/llvm-symbolizer-18: error: '[stack]': No such file or directory",
    "==================",
    "WARNING: ThreadSanitizer: data race (pid=10)",
    "  Write of size 4 at 0x5630b7b76ae8 by thread T2:",
    "    #0 bump() /work/main.cpp:3:45 (a.out+0xe0b19) (BuildId: 24048eb212a82d172b3982a2c5b84c18c6b8bc18)",
    "    #1 void std::__invoke_impl<void, void (*)()>(std::__invoke_other, void (*&&)()) /usr/lib/gcc/x86_64-linux-gnu/14/../../../../include/c++/14/bits/invoke.h:61:14 (a.out+0xe1422) (BuildId: 2404)",
    "    #6 <null> <null> (libstdc++.so.6+0xecdb3) (BuildId: 753c6c8608b61d4e67be8f0c890e03e0aa046b8b)",
    "",
    "  Previous write of size 4 at 0x5630b7b76ae8 by thread T1:",
    "    #0 bump() /work/main.cpp:2:41 (a.out+0xe0b19) (BuildId: 24048eb212a82d172b3982a2c5b84c18c6b8bc18)",
    "",
    "  Location is global 'g' of size 4 at 0x5630b7b76ae8 (a.out+0x150eae8)",
    "",
    "  Thread T2 (tid=13, running) created by main thread at:",
    "    #0 pthread_create <null> (a.out+0x6063f) (BuildId: 24048eb212a82d172b3982a2c5b84c18c6b8bc18)",
    "    #2 main /work/main.cpp:6:17 (a.out+0xe0b75) (BuildId: 24048eb212a82d172b3982a2c5b84c18c6b8bc18)",
    "",
    "SUMMARY: ThreadSanitizer: data race /work/main.cpp:3:45 in bump()",
    "==================",
  );

  it("reads a data race: both accesses, and ignores where the threads were created", () => {
    const r = parseRuntimeReport(race)!;
    expect(r.tool).toBe("ThreadSanitizer");
    expect(r.kind).toBe("data-race");
    expect(r.summary).toBe("ThreadSanitizer: data-race");
    expect(r.access).toBe("WRITE of size 4");
    expect(r.location).toEqual({ file: "main.cpp", line: 3, column: 45, function: "bump()" });
    expect(r.previousAccess).toBe("WRITE of size 4");
    expect(r.previousAt).toMatchObject({ file: "main.cpp", line: 2, column: 41 });
    expect(r.frames.map((f) => f.line)).toEqual([3]); // the library frames and the thread-creation stack are gone
  });

  it("reads a lock-order inversion: the two acquisitions, the first in this thread", () => {
    const r = parseRuntimeReport(
      lines(
        "==================",
        "WARNING: ThreadSanitizer: lock-order-inversion (potential deadlock) (pid=10)",
        "  Cycle in lock order graph: M0 (0x7ffc7a4322e8) => M1 (0x7ffc7a4322c0) => M0",
        "",
        "  Mutex M1 acquired here while holding mutex M0 in thread T1:",
        "    #0 pthread_mutex_lock <null> (a.out+0x6317e) (BuildId: e4b5)",
        "    #3 std::lock_guard<std::mutex>::lock_guard(std::mutex&) /usr/lib/gcc/x86_64-linux-gnu/14/../../../../include/c++/14/bits/std_mutex.h:250:19 (a.out+0xe2dc9) (BuildId: e4b5)",
        "    #4 main::$_0::operator()() const /work/main.cpp:5:64 (a.out+0xe22bc) (BuildId: e4b5)",
        "",
        "    Hint: use TSAN_OPTIONS=second_deadlock_stack=1 to get more informative warning message",
        "",
        "  Mutex M0 acquired here while holding mutex M1 in thread T2:",
        "    #4 main::$_1::operator()() const /work/main.cpp:7:64 (a.out+0xe291c) (BuildId: e4b5)",
        "",
        "SUMMARY: ThreadSanitizer: lock-order-inversion (potential deadlock) (/work/a.out+0x6317e) in pthread_mutex_lock",
      ),
    )!;
    expect(r).toMatchObject({ tool: "ThreadSanitizer", kind: "lock-order-inversion", summary: "ThreadSanitizer: lock-order-inversion" });
    expect(r.location).toMatchObject({ file: "main.cpp", line: 5 });
    expect(r.previousAt).toMatchObject({ file: "main.cpp", line: 7 });
  });

  it("prefers the report that came first when several sanitizers speak", () => {
    const both = lines("main.cpp:9:3: runtime error: signed integer overflow: 1 + 2147483647 cannot be represented in type 'int'", race);
    expect(parseRuntimeReport(both)?.tool).toBe("UndefinedBehaviorSanitizer");
  });
});

// Fixtures are trimmed from output captured from the real sandbox image (clang 18 + the sanitizer runtimes).
describe("parseRuntimeReport", () => {
  const uaf = lines(
    "=================================================================",
    "==10==ERROR: AddressSanitizer: heap-use-after-free on address 0x502000000010 at pc 0x56198f2e4c83 bp 0x7ffcda8585e0 sp 0x7ffcda8585d8",
    "READ of size 4 at 0x502000000010 thread T0",
    "    #0 0x56198f2e4c82 in main /work/main.cpp:4:12",
    "    #1 0x7f7f7d8d51c9  (/lib/x86_64-linux-gnu/libc.so.6+0x2a1c9) (BuildId: a4a7992a8e66555c8141ab2a08a8465ff6e0ea65)",
    "",
    "0x502000000010 is located 0 bytes inside of 4-byte region [0x502000000010,0x502000000014)",
    "freed by thread T0 here:",
    "    #0 0x56198f2e3b91 in operator delete(void*) (/work/a.out+0x1b91) (BuildId: 8ab1)",
    "    #1 0x56198f2e4c15 in main /work/main.cpp:3:5",
    "",
    "previously allocated by thread T0 here:",
    "    #0 0x56198f2e2f01 in operator new(unsigned long) (/work/a.out+0x1f01) (BuildId: 8ab1)",
    "    #1 0x56198f2e4b50 in main /work/main.cpp:2:14",
    "",
    "SUMMARY: AddressSanitizer: heap-use-after-free /work/main.cpp:4:12 in main",
  );

  it("reads a use-after-free: kind, access, where it happened, where it was freed and allocated", () => {
    const r = parseRuntimeReport(uaf)!;
    expect(r.tool).toBe("AddressSanitizer");
    expect(r.kind).toBe("heap-use-after-free");
    expect(r.summary).toBe("AddressSanitizer: heap-use-after-free");
    expect(r.access).toBe("READ of size 4");
    expect(r.location).toEqual({ file: "main.cpp", line: 4, column: 12, function: "main" });
    expect(r.freedAt).toMatchObject({ file: "main.cpp", line: 3, column: 5 });
    expect(r.allocatedAt).toMatchObject({ file: "main.cpp", line: 2, column: 14 });
    expect(r.frames.every((f) => f.user)).toBe(true);
  });

  it("reads a double free and an alloc-dealloc mismatch", () => {
    const d = parseRuntimeReport(lines("==10==ERROR: AddressSanitizer: attempting double-free on 0x502000000010 in thread T0:", "    #1 0x559 in main /work/main.cpp:4:5"));
    expect(d).toMatchObject({ kind: "double-free", summary: "AddressSanitizer: double-free" });
    expect(d?.location?.line).toBe(4);
    const m = parseRuntimeReport(lines("==10==ERROR: AddressSanitizer: alloc-dealloc-mismatch (operator new [] vs operator delete) on 0x502000000010", "    #1 0x55c in main /work/main.cpp:3:5"));
    expect(m?.kind).toBe("alloc-dealloc-mismatch");
  });

  it("reads a leak (the allocation site is in the learner's code)", () => {
    const r = parseRuntimeReport(
      lines(
        "==10==ERROR: LeakSanitizer: detected memory leaks",
        "",
        "Direct leak of 40 byte(s) in 1 object(s) allocated from:",
        "    #0 0x55e52f4d28d1 in operator new[](unsigned long) (/work/a.out+0x1058d1) (BuildId: 8357)",
        "    #1 0x55e52f4d4b53 in main /work/main.cpp:2:39",
        "",
        "SUMMARY: AddressSanitizer: 40 byte(s) leaked in 1 allocation(s).",
      ),
    );
    expect(r).toMatchObject({ tool: "LeakSanitizer", kind: "leak", summary: "LeakSanitizer: leak" });
    expect(r?.allocatedAt).toMatchObject({ file: "main.cpp", line: 2 });
  });

  it("reads undefined behavior reports", () => {
    const o = parseRuntimeReport(
      lines(
        "main.cpp:6:9: runtime error: signed integer overflow: 2147483647 + 1 cannot be represented in type 'int'",
        "    #0 0x5 in main /work/main.cpp:6:9",
        "SUMMARY: UndefinedBehaviorSanitizer: undefined-behavior main.cpp:6:9",
      ),
    )!;
    expect(o.tool).toBe("UndefinedBehaviorSanitizer");
    expect(o.kind).toBe("signed-integer-overflow");
    expect(o.summary).toContain("signed integer overflow");
    expect(o.location).toMatchObject({ file: "main.cpp", line: 6, column: 9 });
    expect(parseRuntimeReport("main.cpp:1:40: runtime error: division by zero")?.kind).toBe("division-by-zero");
    expect(parseRuntimeReport("main.cpp:3:12: runtime error: load of null pointer of type 'int'")?.kind).toBe("load-of-null-pointer");
    expect(parseRuntimeReport("main.cpp:3:12: runtime error: index 5 out of bounds for type 'int[4]'")?.kind).toBe("index");
  });

  it("returns null when there is no report, and the first one when there are several", () => {
    expect(parseRuntimeReport(lines("just some output", "and a warning: nothing"))).toBeNull();
    const two = lines("main.cpp:2:3: runtime error: division by zero", "==1==ERROR: AddressSanitizer: heap-use-after-free on address 0x1");
    expect(parseRuntimeReport(two)?.kind).toBe("division-by-zero");
  });

  it("does not present frames from the library or the test framework as the learner's code", () => {
    const r = parseRuntimeReport(
      lines(
        "==1==ERROR: AddressSanitizer: heap-buffer-overflow on address 0x1 at pc 0x2",
        "READ of size 4 at 0x1 thread T0",
        "    #0 0x1 in std::vector<int>::operator[](unsigned long) /usr/include/c++/14/bits/stl_vector.h:1125:9",
        "    #1 0x2 in sum /work/main.cpp:2:96",
        "    #2 0x3 in zth::run_all() /opt/zth/include/zth_test.hpp:503:13",
      ),
    )!;
    expect(r.frames).toHaveLength(1);
    expect(r.location).toMatchObject({ file: "main.cpp", line: 2 });
  });
});
