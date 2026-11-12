import { describe, expect, it } from "vitest";
import { complete, readSse } from "./api";
import type { CompleteOptions } from "./api";
import { buildReference, checkReply, extractCodeBlocks, FALLBACK_MESSAGE } from "./guardrail";
import { MODES, modeInfo } from "./modes";
import { buildSystem } from "./prompt";
import type { TutorContext } from "./prompt";
import { sanitize } from "./settings";
import { ask } from "./session";

const ctx: TutorContext = {
  title: "Counting words",
  lessonText: "Split the text on spaces.",
  code: { "main.cpp": "int count(const std::string& s) { return 0; }" },
  diagnostics: ["main.cpp:3:5: error: expected ';'"],
  failingTests: [{ name: "counts two words", message: "expected 2, got 0" }],
  hintsUsed: 1,
  aiHelpUsed: 0,
  complete: false,
};

const SOLUTION = {
  "main.cpp": [
    "#include <string>",
    "int count(const std::string& s) {",
    "    int words = 0;",
    "    bool inWord = false;",
    "    for (char c : s) {",
    "        if (c != ' ' && !inWord) { ++words; }",
    "        inWord = (c != ' ');",
    "    }",
    "    return words;",
    "}",
  ].join("\n"),
};
const STARTER = { "main.cpp": "#include <string>\nint count(const std::string& s) {\n    return 0;\n}\n" };

describe("modes", () => {
  it("has the six spec modes, plus discuss and quiz", () => {
    expect(MODES.map((m) => m.id)).toEqual(["nudge", "concept", "error", "pinpoint", "approach", "discuss", "review", "quiz"]);
  });
  it("only review may write solution code", () => {
    expect(MODES.filter((m) => !m.guarded).map((m) => m.id)).toEqual(["review"]);
    expect(MODES.filter((m) => m.needsComplete).map((m) => m.id)).toEqual(["review", "quiz"]);
  });
  it("discuss forbids stating the answer", () => {
    expect(modeInfo("discuss").rule).toMatch(/never state the answer/);
  });
});

describe("buildSystem", () => {
  it("includes the lesson, code, failing tests and diagnostics", () => {
    const s = buildSystem("nudge", ctx);
    for (const part of ["Counting words", "Split the text on spaces.", "int count(", "counts two words: expected 2, got 0", "expected ';'", "Hints used so far: 1", "NUDGE"]) expect(s).toContain(part);
  });
  it("contains nothing from the reference solution (it is not even an input)", () => {
    const s = buildSystem("pinpoint", ctx);
    expect(s).not.toContain("inWord");
    expect(Object.keys(ctx)).not.toContain("solution");
  });
  it("caps huge inputs", () => {
    const big = buildSystem("nudge", { ...ctx, code: { "a.cpp": "x".repeat(200_000) }, diagnostics: Array(500).fill("d") });
    expect(big.length).toBeLessThan(80_000);
  });
});

describe("extractCodeBlocks", () => {
  it("counts lines per fenced block, including an unterminated one", () => {
    const text = "intro\n```cpp\na\nb\n```\ntext\n```\nx\ny\nz";
    expect(extractCodeBlocks(text).map((b) => b.lines)).toEqual([2, 3]);
  });
  it("is not fooled by a longer fence containing a shorter one", () => {
    const text = "````\n```\ninner\n```\n````\n";
    expect(extractCodeBlocks(text).map((b) => b.lines)).toEqual([3]);
  });
  it("finds nothing in plain text", () => {
    expect(extractCodeBlocks("no code here")).toEqual([]);
  });
});

describe("checkReply", () => {
  const ref = buildReference(SOLUTION, STARTER);
  const fence = (body: string) => "Try this:\n```cpp\n" + body + "\n```\n";

  it("accepts prose and short examples on other problems", () => {
    expect(checkReply("Think about what a word boundary is.", "nudge", ref).ok).toBe(true);
    expect(checkReply(fence("int sum(int a, int b) {\n    return a + b;\n}"), "concept", ref).ok).toBe(true);
  });
  it("rejects a block over 12 lines", () => {
    const v = checkReply(fence(Array.from({ length: 13 }, (_, i) => `int v${i} = ${i};`).join("\n")), "concept", ref);
    expect(v).toMatchObject({ ok: false, reason: "too-long" });
  });
  it("allows exactly 12 lines", () => {
    expect(checkReply(fence(Array.from({ length: 12 }, (_, i) => `int v${i} = ${i};`).join("\n")), "concept", null).ok).toBe(true);
  });
  it("rejects a short block that reproduces the solution, however it is indented or commented", () => {
    const v = checkReply(fence("  int words = 0;  // counter\n  bool inWord = false;\n  inWord = (c != ' ');"), "approach", ref);
    expect(v).toMatchObject({ ok: false, reason: "matches-solution" });
  });
  it("does not count lines the starter already gave", () => {
    expect(checkReply(fence("int count(const std::string& s) {\n    return 0;\n}"), "nudge", ref).ok).toBe(true);
  });
  it("catches a tiny solution that is a single distinctive line", () => {
    const hello = 'std::cout << "Hello, World!\\n";';
    const tiny = buildReference({ "main.cpp": `#include <iostream>\nint main() {\n    ${hello}\n}\n` }, { "main.cpp": "#include <iostream>\nint main() {\n}\n" });
    expect(tiny.lines.size).toBe(1);
    expect(checkReply(fence(hello), "nudge", tiny).ok).toBe(false);
    expect(checkReply(fence("std::cout << value << std::endl;"), "nudge", tiny).ok).toBe(true);
    expect(checkReply(fence("int a = 1;"), "nudge", { lines: new Set() }).ok).toBe(true);
  });
  it("does not check review mode", () => {
    expect(checkReply(fence(SOLUTION["main.cpp"]), "review", ref).ok).toBe(true);
  });
  it("guards discuss and quiz like the other hint modes", () => {
    expect(checkReply(fence(SOLUTION["main.cpp"]), "discuss", ref).ok).toBe(false);
    expect(checkReply(fence(SOLUTION["main.cpp"]), "quiz", ref).ok).toBe(false);
  });
});

const reply = (text: string) => ({ text, usage: { input: 10, output: 5 }, stopReason: "end_turn" });
const base = { apiKey: "k", model: "m", maxTokens: 100, question: "help", history: [], context: ctx, reference: buildReference(SOLUTION, STARTER) };
const leaky = "```cpp\n" + SOLUTION["main.cpp"] + "\n```";

describe("ask", () => {
  it("returns a clean reply once, summing usage", async () => {
    let calls = 0;
    const r = await ask({ ...base, mode: "nudge", send: async () => (calls++, reply("What counts as a word boundary?")) });
    expect(r).toMatchObject({ text: "What counts as a word boundary?", fellBack: false, usage: { input: 10, output: 5 } });
    expect(calls).toBe(1);
  });
  it("re-asks once after a leak and accepts a clean second reply", async () => {
    const seen: CompleteOptions[] = [];
    const leaks: string[] = [];
    const r = await ask({
      ...base,
      mode: "pinpoint",
      onLeak: (v) => leaks.push(v.reason),
      send: async (o) => (seen.push(o), reply(seen.length === 1 ? leaky : "Look at how you treat spaces.")),
    });
    expect(r).toMatchObject({ text: "Look at how you treat spaces.", fellBack: false, usage: { input: 20, output: 10 } });
    expect(leaks).toEqual(["matches-solution"]);
    expect(seen[1]?.messages.at(-1)?.content).toMatch(/without writing solution code/);
  });
  it("falls back to a safe message when the second reply also leaks", async () => {
    const r = await ask({ ...base, mode: "approach", send: async () => reply(leaky) });
    expect(r).toMatchObject({ text: FALLBACK_MESSAGE, fellBack: true });
  });
  it("never puts the leaked text of the solution into a request", async () => {
    const bodies: string[] = [];
    await ask({ ...base, mode: "nudge", send: async (o) => (bodies.push(JSON.stringify(o)), reply("ok")) });
    expect(bodies.join("")).not.toContain("inWord");
  });
  it("streams unguarded modes only", async () => {
    const got: Array<unknown> = [];
    await ask({ ...base, mode: "review", send: async (o) => (got.push(o.onText), reply("fine")) });
    await ask({ ...base, mode: "nudge", send: async (o) => (got.push(o.onText), reply("fine")), onText: () => {} });
    expect(typeof got[0] === "function" || got[0] === undefined).toBe(true);
    expect(got[1]).toBeUndefined();
  });
  it("keeps only the recent history", async () => {
    const history = Array.from({ length: 40 }, (_, i) => ({ role: i % 2 ? ("assistant" as const) : ("user" as const), content: `m${i}` }));
    let n = 0;
    await ask({ ...base, history, mode: "nudge", send: async (o) => ((n = o.messages.length), reply("ok")) });
    expect(n).toBe(17);
  });
});

function sse(events: Array<Record<string, unknown>>, chunk = 7): Response {
  const text = events.map((e) => `event: ${String(e.type)}\ndata: ${JSON.stringify(e)}\n\n`).join("");
  const bytes = new TextEncoder().encode(text);
  const body = new ReadableStream<Uint8Array>({
    start(c) {
      for (let i = 0; i < bytes.length; i += chunk) c.enqueue(bytes.slice(i, i + chunk));
      c.close();
    },
  });
  return new Response(body, { status: 200 });
}

describe("complete (streaming client)", () => {
  const events = [
    { type: "message_start", message: { usage: { input_tokens: 42, output_tokens: 1 } } },
    { type: "content_block_delta", delta: { type: "text_delta", text: "Hel" } },
    { type: "content_block_delta", delta: { type: "text_delta", text: "lo é" } },
    { type: "message_delta", delta: { stop_reason: "end_turn" }, usage: { output_tokens: 9 } },
  ];
  it("assembles text and usage even when the stream is split mid-event", async () => {
    const parts: string[] = [];
    const seen: Array<{ url: string; init: RequestInit }> = [];
    const r = await complete({
      apiKey: "sk-ant-test",
      model: "claude-sonnet-5-5",
      maxTokens: 50,
      system: "S",
      messages: [{ role: "user", content: "hi" }],
      onText: (d) => parts.push(d),
      fetchImpl: (async (url: string, init: RequestInit) => (seen.push({ url, init }), sse(events))) as unknown as typeof fetch,
    });
    expect(r).toEqual({ text: "Hello é", usage: { input: 42, output: 9 }, stopReason: "end_turn" });
    expect(parts.join("")).toBe("Hello é");
    expect(seen[0]?.url).toBe("https://api.anthropic.com/v1/messages");
    const h = seen[0]?.init.headers as Record<string, string>;
    expect(h["x-api-key"]).toBe("sk-ant-test");
    expect(h["anthropic-dangerous-direct-browser-access"]).toBe("true");
    expect(h["anthropic-version"]).toBe("2023-06-01");
    expect(JSON.parse(String(seen[0]?.init.body))).toMatchObject({ model: "claude-sonnet-5-5", max_tokens: 50, system: "S", stream: true });
  });
  it("turns HTTP errors into plain messages", async () => {
    const f = (async () => new Response(JSON.stringify({ error: { message: "bad key" } }), { status: 401 })) as unknown as typeof fetch;
    await expect(complete({ apiKey: "x", model: "m", maxTokens: 1, system: "", messages: [], fetchImpl: f })).rejects.toThrow(/key was rejected/);
    const g = (async () => new Response("{}", { status: 429 })) as unknown as typeof fetch;
    await expect(complete({ apiKey: "x", model: "m", maxTokens: 1, system: "", messages: [], fetchImpl: g })).rejects.toThrow(/Rate limit/);
  });
  it("reports a network failure", async () => {
    const f = (async () => {
      throw new TypeError("failed");
    }) as unknown as typeof fetch;
    await expect(complete({ apiKey: "x", model: "m", maxTokens: 1, system: "", messages: [], fetchImpl: f })).rejects.toThrow(/Could not reach/);
  });
  it("surfaces an error event in the stream", async () => {
    const f = (async () => sse([{ type: "error", error: { type: "overloaded_error", message: "busy" } }])) as unknown as typeof fetch;
    await expect(complete({ apiKey: "x", model: "m", maxTokens: 1, system: "", messages: [], fetchImpl: f })).rejects.toThrow(/overloaded/);
  });
  it("readSse handles CRLF and multi-line data", async () => {
    const body = new Response("event: a\r\ndata: 1\r\ndata: 2\r\n\r\n").body as ReadableStream<Uint8Array>;
    const out: Array<{ event: string; data: string }> = [];
    for await (const e of readSse(body)) out.push(e);
    expect(out).toEqual([{ event: "a", data: "1\n2" }]);
  });
});

describe("sanitize (settings)", () => {
  it("clamps and cleans stored values", () => {
    expect(sanitize({ apiKey: "  sk-ant-x  ", model: "claude-sonnet-5-5", maxTokens: 999999 })).toEqual({ apiKey: "sk-ant-x", model: "claude-sonnet-5-5", maxTokens: 4096 });
    expect(sanitize({ model: "bad model!", maxTokens: "x" }).model).toBe("claude-sonnet-5-5");
    expect(sanitize(null).maxTokens).toBe(1024);
    expect(sanitize({ maxTokens: 1 }).maxTokens).toBe(64);
  });
});
