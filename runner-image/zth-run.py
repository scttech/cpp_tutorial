#!/usr/bin/env python3
"""Reads one job (a single JSON line) on stdin, compiles + runs it, writes one JSON result to stdout.

job = {
  "files": {"main.cpp": "..."}, "testFiles": {...}, "flags": [...],
  "stdin": "", "mode": "run" | "test",
  "limits": {"wallMs": 5000, "maxOutputBytes": 65536, "compileMs": 30000}
}
Runs inside a throwaway, network-less container; the work dir is a tmpfs.
"""
import json, os, resource, selectors, signal, subprocess, sys, time

WORK = "/work"
DEFAULT_FLAGS = ["-std=c++20", "-Wall", "-Wextra"]
SAFE_NAME = lambda n: n and not n.startswith("/") and ".." not in n.split("/")


def write_files(files):
    for name, content in files.items():
        if not SAFE_NAME(name):
            raise ValueError(f"unsafe file name: {name}")
        path = os.path.join(WORK, name)
        os.makedirs(os.path.dirname(path), exist_ok=True)
        with open(path, "w", encoding="utf-8") as f:
            f.write(content)


def run_capped(cmd, stdin_data, timeout_s, max_out):
    """Run cmd; stream stdout/stderr keeping at most max_out bytes each.

    Output beyond the cap is discarded and the process is killed, so a program
    that prints gigabytes cannot exhaust this container's memory.
    """
    def limits():
        os.setsid()
        resource.setrlimit(resource.RLIMIT_CORE, (0, 0))
        resource.setrlimit(resource.RLIMIT_FSIZE, (16 * 1024 * 1024,) * 2)
    p = subprocess.Popen(cmd, stdin=subprocess.PIPE, stdout=subprocess.PIPE,
                         stderr=subprocess.PIPE, preexec_fn=limits, cwd=WORK)
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


def build(flags, user_sources, test_sources, compile_s):
    """Compile + link. Returns (ok, compiler_stderr, timed_out).

    Run mode: one clang++ invocation. Test mode: the learner's files are compiled normally and
    then their `main` SYMBOL is renamed to `zth_user_main` (objcopy) so the hidden tests can call
    and capture it, while the test sources provide the real main. (Renaming with -Dmain= would
    break the rule that main implicitly returns 0.)
    """
    base = ["clang++", *flags, "-I/opt/zth/include", "-I" + WORK]
    if not test_sources:
        r = run_capped([*base, *user_sources, "-o", "/work/a.out"], "", compile_s, 262144)
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
    r = run_capped([*base, *objs, "-o", "/work/a.out"], "", max(deadline - time.monotonic(), 1), 262144)
    return r["exitCode"] == 0 and not r["timedOut"], output + r["stderr"], r["timedOut"]


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
    flags = job.get("flags") or DEFAULT_FLAGS
    test_mode = job.get("mode") == "test"
    files, tests = job.get("files", {}), job.get("testFiles", {})
    result = {"compiled": False, "compileOutput": "", "stdout": "", "stderr": "",
              "exitCode": None, "signal": None, "timedOut": False,
              "compileMs": 0, "runMs": 0}
    try:
        write_files(files)
        if test_mode:
            write_files(tests)
        user_sources = [n for n in files if is_source(n)]
        test_sources = [n for n in tests if is_source(n)] if test_mode else []
        if not user_sources and not test_sources:
            result["compileOutput"] = "no .cpp files to compile"
        else:
            t0 = time.monotonic()
            ok, output, _ = build(flags, user_sources, test_sources,
                                  int(lim.get("compileMs", 30000)) / 1000)
            result["compileMs"] = int((time.monotonic() - t0) * 1000)
            result["compileOutput"] = output
            if ok:
                result["compiled"] = True
                t1 = time.monotonic()
                r = run_capped(["/work/a.out"], job.get("stdin", ""),
                               int(lim.get("wallMs", 5000)) / 1000, max_out)
                result["runMs"] = int((time.monotonic() - t1) * 1000)
                result.update(r)
    except Exception as e:  # job-level failure, not a user-code failure
        result["internalError"] = f"{type(e).__name__}: {e}"
    json.dump(result, sys.stdout)


main()
