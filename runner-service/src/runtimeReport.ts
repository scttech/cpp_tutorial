import type { Frame, RuntimeReport } from "./types.js";

// Sanitizer headers (AddressSanitizer / LeakSanitizer / ThreadSanitizer):
//   ==14==ERROR: AddressSanitizer: heap-buffer-overflow on address 0x... at pc 0x...
//   ==14==ERROR: AddressSanitizer: attempting double-free on 0x... in thread T0:
//   ==14==ERROR: LeakSanitizer: detected memory leaks
const ERROR_LINE = /==\d+==ERROR: (AddressSanitizer|LeakSanitizer|ThreadSanitizer): (.*)$/m;
// UBSan: main.cpp:6:9: runtime error: signed integer overflow: 2147483647 + 1 cannot be represented in type 'int'
const UBSAN_LINE = /^(.+?):(\d+):(\d+): runtime error: (.*)$/m;
//     #1 0x562f0fa5afed in zth_fn_8() /work/test.cpp:8:70           (a frame; the function name may contain spaces)
const FRAME = /^\s*#\d+ 0x[0-9a-f]+ (?:in (.+?) )?(\/\S+?):(\d+)(?::(\d+))?\s*(?:\(BuildId.*)?$/;
const ACCESS = /^(READ|WRITE) of size (\d+)/;

/** Where the learner's code lives. Frames elsewhere (the standard library, the test framework) are not interesting. */
const isUserFile = (file: string) => file.startsWith("/work/") && !file.startsWith("/work/obj/");
const shortFile = (file: string) => file.replace(/^\/work\//, "");

function toFrame(line: string): Frame | null {
  const m = FRAME.exec(line);
  if (!m) return null;
  return { function: m[1] ?? "", file: shortFile(m[2]!), line: Number(m[3]), column: m[4] ? Number(m[4]) : 0, user: isUserFile(m[2]!) };
}

/** The kind of problem as one lowercase word: heap-use-after-free, double-free, leak, ... */
function asanKind(rest: string): string {
  if (/^attempting double-free/.test(rest)) return "double-free";
  if (/^attempting free on address which was not malloc/.test(rest)) return "bad-free";
  if (/^alloc-dealloc-mismatch/.test(rest)) return "alloc-dealloc-mismatch";
  if (/^SEGV on unknown address/.test(rest)) return "segv";
  if (/^detected memory leaks/.test(rest)) return "leak";
  if (/^data race/.test(rest)) return "data-race";
  return /^[\w-]+/.exec(rest)?.[0] ?? rest;
}

/** UBSan has no kind field: derive one from the start of the message (the part before the numbers). */
function ubsanKind(message: string): string {
  const m = /^(signed integer overflow|unsigned integer overflow|division by zero|shift exponent|left shift of|load of null pointer|store to null pointer|member access within null pointer|member call on null pointer|reference binding to null pointer|index|load of value|variable length array|null pointer passed as argument|downcast of|call to function|applying non-zero offset|applying zero offset|pointer index expression|negation of|execution reached)/.exec(message);
  return (m?.[1] ?? message.split(/[:,]/)[0] ?? message).trim().replace(/\s+/g, "-");
}

/**
 * Pull the FIRST sanitizer report out of a program's stderr, or null if there is none. Frames are cut down to what a
 * learner can act on: the access, where it happened, and (for memory errors) where the memory was allocated and freed.
 */
export function parseRuntimeReport(stderr: string): RuntimeReport | null {
  const lines = stderr.split("\n");
  const ub = UBSAN_LINE.exec(stderr);
  const asan = ERROR_LINE.exec(stderr);
  // whichever report came first in the output
  if (ub && (!asan || ub.index < asan.index)) {
    const [, file, line, column, message] = ub;
    const frames: Frame[] = [];
    const start = lines.findIndex((l) => l.includes("runtime error:"));
    for (const l of lines.slice(start + 1)) {
      const f = toFrame(l);
      if (f) frames.push(f);
      else if (frames.length) break;
    }
    return {
      tool: "UndefinedBehaviorSanitizer",
      kind: ubsanKind(message!),
      summary: `UndefinedBehaviorSanitizer: ${message}`,
      location: { file: shortFile(file!), line: Number(line), column: Number(column), function: "" },
      frames: frames.filter((f) => f.user).slice(0, 5),
    };
  }
  if (!asan) return null;

  const tool = asan[1] as RuntimeReport["tool"];
  const rest = asan[2]!;
  const kind = asanKind(rest);
  let access: string | undefined;
  const sections: Record<"error" | "freed" | "allocated", Frame[]> = { error: [], freed: [], allocated: [] };
  let current: "error" | "freed" | "allocated" = "error";
  const startLine = lines.findIndex((l) => ERROR_LINE.test(l));
  for (const l of lines.slice(startLine + 1)) {
    const a = ACCESS.exec(l);
    if (a && !access) access = `${a[1]} of size ${a[2]}`;
    if (/^freed by thread/.test(l)) current = "freed";
    else if (/^previously allocated by thread/.test(l) || /^allocated by thread/.test(l) || /^Direct leak of/.test(l) || /^Indirect leak of/.test(l)) current = "allocated";
    else if (/^SUMMARY:/.test(l)) break;
    const f = toFrame(l);
    if (f) sections[current].push(f);
  }
  const user = (fs: Frame[]) => fs.filter((f) => f.user).slice(0, 5);
  const report: RuntimeReport = {
    tool,
    kind,
    summary: `${tool}: ${kind}`,
    frames: user(sections.error),
  };
  if (access) report.access = access;
  const first = report.frames[0];
  if (first) report.location = { file: first.file, line: first.line, column: first.column, function: first.function };
  if (sections.freed.length) report.freedAt = user(sections.freed)[0];
  if (sections.allocated.length) report.allocatedAt = user(sections.allocated)[0];
  return report;
}
