#!/usr/bin/env python3
"""Reads one job (a single JSON line) on stdin, compiles + runs it, writes one JSON result to stdout.

job = {
  "files": {"main.cpp": "..."}, "testFiles": {...}, "flags": [...],
  "stdin": "", "mode": "run" | "test" | "grade",
  "libs": ["gtest", "gmock", "catch2", "doctest"],           # test frameworks to link
  "analyzers": [{"tool": "clang-tidy" | "cppcheck" | "clang-format", "files": [...], "checks": "...", "config": "file"}],
  "limits": {"wallMs": 5000, "maxOutputBytes": 65536, "compileMs": 30000}
}
Runs inside a throwaway, network-less container; the work dir is a tmpfs.
"""
import json, os, resource, selectors, signal, subprocess, sys, time

WORK = "/work"
DEFAULT_FLAGS = ["-std=c++20", "-Wall", "-Wextra"]

# Sanitizers (-fsanitize=...): a report ends the program with this exit code, so the test harness and the service can
# tell "a sanitizer stopped it" from an ordinary failure. Reports are symbolized (file:line), and leaks are reported.
SANITIZER_EXIT_CODE = 77
SYMBOLIZER = "/usr/bin/llvm-symbolizer-18"
SANITIZER_ENV = {
    "ASAN_OPTIONS": f"exitcode={SANITIZER_EXIT_CODE}:detect_leaks=1:symbolize=1:external_symbolizer_path={SYMBOLIZER}"
                    ":detect_stack_use_after_return=1:check_initialization_order=1:strict_string_checks=1:allocator_may_return_null=0",
    "UBSAN_OPTIONS": f"exitcode={SANITIZER_EXIT_CODE}:halt_on_error=1:print_stacktrace=1:symbolize=1:external_symbolizer_path={SYMBOLIZER}",
    "TSAN_OPTIONS": f"exitcode={SANITIZER_EXIT_CODE}:halt_on_error=1:external_symbolizer_path={SYMBOLIZER}",
}


def sanitizer_env(test_mode):
    """In test mode the tests run in forked children that end with _exit, and the parent only holds the harness's own
    bookkeeping (blocks it deliberately keeps after a delete), so a leak check at its exit would only ever report the
    harness itself. Leaks in tests are found by the harness's allocation tracking; LeakSanitizer is for plain runs."""
    env = {**os.environ, **SANITIZER_ENV}
    if test_mode:
        env["ASAN_OPTIONS"] = env["ASAN_OPTIONS"].replace("detect_leaks=1", "detect_leaks=0")
    return env


def with_sanitizer_flags(flags):
    """Sanitized builds get debug info and frame pointers (readable stack traces), and never 'recover' from a report."""
    if not any(f.startswith("-fsanitize=") for f in flags):
        return flags
    extra = [f for f in ("-g", "-fno-omit-frame-pointer", "-fno-sanitize-recover=all") if f not in flags]
    return [*flags, *extra]
SAFE_NAME = lambda n: n and not n.startswith("/") and ".." not in n.split("/")


def write_files(files):
    for name, content in files.items():
        if not SAFE_NAME(name):
            raise ValueError(f"unsafe file name: {name}")
        path = os.path.join(WORK, name)
        os.makedirs(os.path.dirname(path), exist_ok=True)
        with open(path, "w", encoding="utf-8") as f:
            f.write(content)


def run_capped(cmd, stdin_data, timeout_s, max_out, env=None):
    """Run cmd; stream stdout/stderr keeping at most max_out bytes each.

    Output beyond the cap is discarded and the process is killed, so a program
    that prints gigabytes cannot exhaust this container's memory.
    """
    def limits():
        os.setsid()
        resource.setrlimit(resource.RLIMIT_CORE, (0, 0))
        resource.setrlimit(resource.RLIMIT_FSIZE, (16 * 1024 * 1024,) * 2)
    p = subprocess.Popen(cmd, stdin=subprocess.PIPE, stdout=subprocess.PIPE,
                         stderr=subprocess.PIPE, preexec_fn=limits, cwd=WORK, env=env)
    try:
        p.stdin.write(stdin_data.encode())
        p.stdin.close()
    except (BrokenPipeError, OSError):
        pass
    sel = selectors.DefaultSelector()
    sel.register(p.stdout, selectors.EVENT_READ, "out")
    sel.register(p.stderr, selectors.EVENT_READ, "err")
    bufs = {"out": bytearray(), "err": bytearray()}
    truncated = {"out": False, "err": False}
    deadline = time.monotonic() + timeout_s
    timed_out = False
    killed_for_output = False
    while sel.get_map():
        remaining = deadline - time.monotonic()
        if remaining <= 0:
            timed_out = True
            break
        for key, _ in sel.select(timeout=min(remaining, 0.2)):
            chunk = os.read(key.fileobj.fileno(), 65536)
            if not chunk:
                sel.unregister(key.fileobj)
                continue
            buf = bufs[key.data]
            room = max_out - len(buf)
            if room > 0:
                buf.extend(chunk[:room])
            if len(chunk) > room:
                truncated[key.data] = True
        if any(truncated.values()):
            killed_for_output = True
            break
    if timed_out or killed_for_output:
        try:
            os.killpg(p.pid, signal.SIGKILL)
        except ProcessLookupError:
            pass
    p.wait()
    text = lambda k: bufs[k].decode("utf-8", "replace") + (
        "\n[output truncated]" if truncated[k] else "")
    code = p.returncode
    sig = -code if code is not None and code < 0 else None
    return {"stdout": text("out"), "stderr": text("err"),
            "exitCode": None if sig else code,
            "signal": signal.Signals(sig).name if sig else None,
            "timedOut": timed_out}


def is_source(name):
    return name.endswith((".cpp", ".cc", ".cxx"))


def link_flags(libs):
    """Linker flags for the test frameworks a lesson asks for (doctest is header-only). Order matters for static libraries."""
    out = []
    if "gmock" in libs:
        out.append("-lgmock")
    if "gtest" in libs or "gmock" in libs:
        out += ["-lgtest", "-lgtest_main", "-pthread"]
    if "catch2" in libs:
        out += ["-lCatch2Main", "-lCatch2"]
    return out


def build(flags, user_sources, test_sources, compile_s, libs=()):
    """Compile + link. Returns (ok, compiler_stderr, timed_out).

    Run mode: one clang++ invocation. Test mode: the learner's files are compiled normally and
    then their `main` SYMBOL is renamed to `zth_user_main` (objcopy) so the hidden tests can call
    and capture it, while the test sources provide the real main. (Renaming with -Dmain= would
    break the rule that main implicitly returns 0.)
    """
    base = ["clang++", *flags, "-isystem/opt/zth/include", "-I" + WORK]
    if not test_sources:
        r = run_capped([*base, *user_sources, *link_flags(libs), "-o", "/work/a.out"], "", compile_s, 262144)
        return r["exitCode"] == 0 and not r["timedOut"], r["stderr"], r["timedOut"]
    os.makedirs(os.path.join(WORK, "obj"), exist_ok=True)
    objs, output = [], ""
    deadline = time.monotonic() + compile_s
    units = [(n, True) for n in user_sources] + [(n, False) for n in test_sources]
    for i, (name, is_user) in enumerate(units):
        obj = f"/work/obj/{i}.o"
        r = run_capped([*base, "-c", name, "-o", obj], "",
                       max(deadline - time.monotonic(), 1), 262144)
        output += r["stderr"]
        if r["timedOut"] or r["exitCode"] != 0:
            return False, output, r["timedOut"]
        if is_user:
            rn = run_capped(["objcopy", "--redefine-sym", "main=zth_user_main", obj], "", 10, 4096)
            if rn["exitCode"] != 0:
                return False, output + rn["stderr"], False
        objs.append(obj)
    r = run_capped([*base, *objs, *link_flags(libs), "-o", "/work/a.out"], "", max(deadline - time.monotonic(), 1), 262144)
    return r["exitCode"] == 0 and not r["timedOut"], output + r["stderr"], r["timedOut"]


# ---- analyzers: clang-tidy, cppcheck, clang-format ---------------------------------------------------------------------

import re

FINDING = re.compile(r"^(?:/work/)?([^\s:][^:\n]*):(\d+):(\d+): (warning|error|note|style|performance|portability|information): (.*)$")
MAX_FINDINGS = 30
NAME_OK = re.compile(r"^[A-Za-z0-9_][A-Za-z0-9_\-./]*$")
CHECKS_OK = re.compile(r"^[A-Za-z0-9*,\-_.+ ]+$")


def protocol_line(name, tier, status, msg=None):
    ev = {"event": "test", "name": name, "tier": tier, "status": status}
    if msg:
        ev["msg"] = msg
    return "@@ZTH " + json.dumps(ev)


def normalize_findings(text):
    """Keep only 'file:line:col: severity: message' lines, with /work/ removed and every severity turned into warning/error/note."""
    kept = []
    for line in text.splitlines():
        m = FINDING.match(line)
        if not m:
            continue
        file, ln, col, sev, msg = m.groups()
        sev = sev if sev in ("error", "note") else "warning"
        kept.append((sev, f"{file}:{ln}:{col}: {sev}: {msg}"))
    return kept


def analyzer_command(spec, flags):
    tool, files = spec["tool"], spec["files"]
    if tool == "clang-tidy":
        cmd = ["clang-tidy", "-quiet", "--warnings-as-errors="]
        if spec.get("config"):
            cmd.append("--config-file=" + spec["config"])
        elif spec.get("checks"):
            cmd.append("--checks=" + spec["checks"])
        # Report findings in the learner's own files only, never in the framework or standard headers.
        cmd.append("--header-filter=/work/(" + "|".join(re.escape(f) for f in files) + ")$")
        std = [f for f in flags if f.startswith(("-std=", "-D"))]
        return [*cmd, *[f for f in files if is_source(f)], "--", *std, "-isystem/opt/zth/include", "-I" + WORK]
    if tool == "cppcheck":
        return ["cppcheck", "--quiet", "--enable=" + (spec.get("checks") or "warning,style,performance,portability"), "--inline-suppr",
                "--suppress=missingIncludeSystem",
                "--suppress=unmatchedSuppression", "--template={file}:{line}:{column}: {severity}: {message} [{id}]",
                *["-" + f for f in flags if f.startswith("-std=")], *files]
    if tool == "clang-format":
        style = "file:" + spec["config"] if spec.get("config") else "LLVM"
        return ["clang-format", "--dry-run", "-Werror", "--style=" + style, *files]
    raise ValueError(f"unknown analyzer: {tool}")


def run_analyzers(analyzers, flags, budget_s):
    """Run each analyzer over its files. Returns (protocol lines, findings text for the Problems list)."""
    lines, findings = [], []
    deadline = time.monotonic() + budget_s
    for spec in analyzers:
        if not spec.get("files") or not all(NAME_OK.match(f) for f in spec["files"]):
            raise ValueError("analyzer needs a list of safe file names")
        if spec.get("checks") and not CHECKS_OK.match(spec["checks"]):
            raise ValueError("unsafe analyzer checks")
        if spec.get("config") and not NAME_OK.match(spec["config"]):
            raise ValueError("unsafe analyzer config name")
        name = spec["tool"] + ": no findings"
        r = run_capped(analyzer_command(spec, flags), "", max(deadline - time.monotonic(), 1), 262144)
        if r["timedOut"]:
            lines.append(protocol_line(name, "clean", "timeout", "the analyzer took too long"))
            continue
        found = normalize_findings(r["stdout"] + "\n" + r["stderr"])
        problems = [t for sev, t in found if sev != "note"]
        findings.extend(t for _, t in found[:MAX_FINDINGS])  # a flood of findings would only bury the first ones; the count is in the result
        if problems:
            lines.append(protocol_line(name, "clean", "fail", f"{len(problems)} finding(s). First: " + problems[0]))
        elif r["exitCode"] not in (0, None):
            lines.append(protocol_line(name, "clean", "error", (r["stderr"] or r["stdout"])[:300]))
        else:
            lines.append(protocol_line(name, "clean", "pass"))
    return lines, "\n".join(findings)


# ---- grade mode: the learner's tests are graded against the correct code and against hidden buggy copies -----------------

def grade(files, tests, flags, libs, lim, result, env):
    """tests/grade.json lists the variants; each one overlays files from tests/variants/<name>/. The learner's tests must PASS on the
    correct code and FAIL on every variant (a variant the tests do not notice is a bug the tests cannot catch)."""
    manifest = json.loads(tests["grade.json"])
    variants = manifest.get("variants", [])
    user_sources = [n for n in files if is_source(n)]
    compile_s = int(lim.get("compileMs", 30000)) / 1000
    wall_s = int(lim.get("wallMs", 5000)) / 1000
    max_out = int(lim.get("maxOutputBytes", 65536))
    t0 = time.monotonic()
    ok, output, _ = build(flags, user_sources, [], compile_s, libs)
    result["compileMs"] = int((time.monotonic() - t0) * 1000)
    result["compileOutput"] = output
    if not ok:
        return []
    result["compiled"] = True
    t1 = time.monotonic()
    base = run_capped(["/work/a.out"], "", wall_s, max_out, env=env)
    result.update(base)
    passed = base["exitCode"] == 0 and not base["timedOut"] and not base["signal"]
    lines = [protocol_line(manifest.get("baseline", "Your tests pass on the correct code"), "works", "pass" if passed else "fail",
                           None if passed else "Your tests fail even though the code under test is correct. Fix the tests first (see the output).")]
    deadline = time.monotonic() + compile_s
    for i, v in enumerate(variants):
        name = v.get("label") or v["name"]
        tier = v.get("tier", "works")
        if not passed:
            lines.append(protocol_line(name, tier, "fail", "Not checked: your tests must pass on the correct code first."))
            continue
        vdir = f"{WORK}/v{i}"
        os.makedirs(vdir, exist_ok=True)
        write_files({f"v{i}/{n}": c for n, c in files.items()})
        prefix = f"variants/{v['name']}/"
        write_files({f"v{i}/{k[len(prefix):]}": c for k, c in tests.items() if k.startswith(prefix)})
        left = deadline - time.monotonic()
        if left <= 0:
            lines.append(protocol_line(name, tier, "error", "ran out of time before this check"))
            continue
        built = run_capped(["clang++", *flags, "-isystem/opt/zth/include", "-I" + vdir, *[f"{vdir}/{n}" for n in user_sources], *link_flags(libs),
                            "-o", f"{vdir}/a.out"], "", left, 262144)
        if built["timedOut"] or built["exitCode"] != 0:
            lines.append(protocol_line(name, tier, "error", "your tests do not compile against this variant: " + built["stderr"][:300]))
            continue
        ran = run_capped([f"{vdir}/a.out"], "", min(wall_s, 3), 4096, env=env)
        caught = ran["timedOut"] or bool(ran["signal"]) or ran["exitCode"] != 0
        hint = v.get("hint") or "Add a test that would fail in that case."
        lines.append(protocol_line(name, tier, "pass" if caught else "fail",
                                   None if caught else f"Your tests still pass when {name[0].lower() + name[1:]}. {hint}"))
    if manifest.get("check"):
        # A hidden test program (zth_test.hpp) that exercises the learner's OWN code under test: used when the learner also edits the library.
        # Every source the manifest does not name as the learner's test file belongs to the library.
        lib_sources = [n for n in user_sources if n not in manifest.get("testSources", [])]
        ok2, out2, _ = build(flags, lib_sources, [manifest["check"]], compile_s)
        if not ok2:
            lines.append(protocol_line("Your library builds", "works", "fail", "It does not compile: " + out2[:400]))
        else:
            r2 = run_capped(["/work/a.out"], "", wall_s, max_out, env=env)
            lines.extend(l for l in r2["stdout"].splitlines() if l.startswith("@@ZTH "))
            if r2["timedOut"] or r2["signal"]:
                lines.append(protocol_line("Your library does not crash or hang", "works", "fail", "The checks ended with " + (r2["signal"] or "a timeout") + "."))
    if manifest.get("clean"):
        warned = any("warning:" in l for l in output.splitlines())
        lines.append(protocol_line("Your tests compile without warnings", "clean", "fail" if warned else "pass", "See the Problems tab." if warned else None))
    result["runMs"] = int((time.monotonic() - t1) * 1000)
    return lines


def main():
    # The job arrives as one line "@@JOB <json>". The docker client library can prepend its own
    # attach options to stdin, so ignore anything before the marker. No EOF is needed (a hijacked
    # docker socket cannot half-close).
    marker = "@@JOB "
    line = sys.stdin.readline()
    while line and marker not in line:
        line = sys.stdin.readline()
    if not line:
        raise SystemExit("no job received on stdin")
    job = json.loads(line.split(marker, 1)[1])
    lim = job.get("limits", {})
    max_out = int(lim.get("maxOutputBytes", 65536))
    flags = with_sanitizer_flags(job.get("flags") or DEFAULT_FLAGS)
    sanitized = any(f.startswith("-fsanitize=") for f in flags)
    mode = job.get("mode")
    test_mode = mode == "test"
    grade_mode = mode == "grade"
    libs = [l for l in job.get("libs", []) if l in ("gtest", "gmock", "catch2", "doctest")]
    analyzers = job.get("analyzers", []) if (test_mode or grade_mode) else []
    files, tests = job.get("files", {}), job.get("testFiles", {})
    result = {"compiled": False, "compileOutput": "", "stdout": "", "stderr": "",
              "exitCode": None, "signal": None, "timedOut": False,
              "compileMs": 0, "runMs": 0}
    try:
        write_files(files)
        if test_mode or grade_mode:
            write_files(tests)
        user_sources = [n for n in files if is_source(n)]
        test_sources = [n for n in tests if is_source(n)] if test_mode else []
        if not user_sources and not test_sources:
            result["compileOutput"] = "no .cpp files to compile"
        elif grade_mode:
            extra = grade(files, tests, flags, libs, lim, result, sanitizer_env(True) if sanitized else None)
            if result["compiled"]:
                alines, findings = run_analyzers(analyzers, flags, 25)
                result["stdout"] = result["stdout"].rstrip("\n") + "\n" + "\n".join(extra + alines) + "\n"
                if findings:
                    result["compileOutput"] += "\n" + findings
        else:
            t0 = time.monotonic()
            ok, output, _ = build(flags, user_sources, test_sources,
                                  int(lim.get("compileMs", 30000)) / 1000, libs)
            result["compileMs"] = int((time.monotonic() - t0) * 1000)
            result["compileOutput"] = output
            if ok:
                result["compiled"] = True
                t1 = time.monotonic()
                r = run_capped(["/work/a.out"], job.get("stdin", ""),
                               int(lim.get("wallMs", 5000)) / 1000, max_out,
                               env=sanitizer_env(test_mode) if sanitized else None)
                result["runMs"] = int((time.monotonic() - t1) * 1000)
                result.update(r)
                if analyzers:
                    alines, findings = run_analyzers(analyzers, flags, 25)
                    result["stdout"] = result["stdout"].rstrip("\n") + "\n" + "\n".join(alines) + "\n"
                    if findings:
                        result["compileOutput"] += "\n" + findings
    except Exception as e:  # job-level failure, not a user-code failure
        result["internalError"] = f"{type(e).__name__}: {e}"
    json.dump(result, sys.stdout)


main()
