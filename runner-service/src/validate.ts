import { z } from "zod";
import type { Config } from "./config.js";
import type { RunRequest } from "./types.js";

const MODES = ["run", "test", "grade"] as const;

const FILE_NAME = /^[A-Za-z0-9_][A-Za-z0-9_\-./]*$/;
const MAX_FILES = 24;
const MAX_FILE_BYTES = 128 * 1024;

/** Only these compiler flags may be passed through. Anything else is rejected. */
const FLAG_ALLOWLIST: RegExp[] = [
  /^-std=c\+\+(17|20|23|2a|2b)$/,
  /^-W(all|extra|pedantic|error|shadow|conversion|sign-conversion|no-[a-z\-]+|error=[a-z\-]+)$/,
  /^-O[0-3sz]?$/,
  /^-g$/,
  /^-pthread$/,
  /^-fno-(exceptions|rtti|omit-frame-pointer)$/,
  /^-fsanitize=(address|undefined|thread)(,(address|undefined))?$/,
  /^-fno-sanitize-recover=all$/,
  /^-D[A-Za-z_][A-Za-z0-9_]*(=[A-Za-z0-9_.\-]*)?$/,
];

const fileMap = z
  .record(z.string(), z.string().max(MAX_FILE_BYTES))
  .refine((m) => Object.keys(m).length <= MAX_FILES, "too many files")
  .refine((m) => Object.keys(m).every((n) => FILE_NAME.test(n) && !n.split("/").includes("..")), "invalid file name");

export const runBodySchema = z.object({
  mode: z.enum(MODES).default("run"),
  userFiles: fileMap,
  testFiles: fileMap.default({}),
  flags: z
    .array(z.string())
    .max(16)
    .default([])
    .refine((fs) => fs.every((f) => FLAG_ALLOWLIST.some((re) => re.test(f))), "flag not allowed"),
  libs: z.array(z.enum(["gtest", "gmock", "catch2", "doctest"])).max(4).default([]),
  analyzers: z
    .array(
      z.object({
        tool: z.enum(["clang-tidy", "cppcheck", "clang-format"]),
        files: z.array(z.string().regex(FILE_NAME)).min(1).max(MAX_FILES),
        checks: z.string().regex(/^[A-Za-z0-9*,\-_.+ ]+$/).max(500).optional(),
        config: z.string().regex(FILE_NAME).optional(),
      }),
    )
    .max(4)
    .default([]),
  stdin: z.string().max(64 * 1024).optional(),
  limits: z
    .object({
      wallMs: z.number().int().positive().optional(),
      memoryMb: z.number().int().positive().optional(),
      maxOutputBytes: z.number().int().positive().optional(),
    })
    .default({}),
});

export type RunBody = z.infer<typeof runBodySchema>;

/** Fill defaults and clamp limits to the server's ceilings. */
export function normalize(body: RunBody, cfg: Config): RunRequest & { mode: (typeof MODES)[number] } {
  const c = cfg.ceilings;
  return {
    mode: body.mode,
    userFiles: body.userFiles,
    testFiles: body.testFiles,
    flags: body.flags,
    libs: body.libs,
    analyzers: body.analyzers,
    stdin: body.stdin ?? "",
    limits: {
      wallMs: Math.min(body.limits.wallMs ?? 5000, c.wallMs),
      memoryMb: Math.min(body.limits.memoryMb ?? 256, c.memoryMb),
      maxOutputBytes: Math.min(body.limits.maxOutputBytes ?? 65536, c.maxOutputBytes),
    },
  };
}
