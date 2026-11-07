#!/usr/bin/env python3
"""Reads one job (JSON) on stdin, compiles + runs it, writes one JSON result to stdout.

job = {
  "files": {"main.cpp": "..."}, "testFiles": {...}, "flags": [...],
  "stdin": "", "mode": "run" | "test",
  "limits": {"wallMs": 5000, "maxOutputBytes": 65536, "compileMs": 30000}
}
Runs inside a throwaway, network-less container; the work dir is a tmpfs.
"""
import json, os, resource, signal, subprocess, sys, time

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
    """Run cmd; capture stdout/stderr up to max_out bytes each; enforce wall time."""
    def limits():
        os.setsid()
        resource.setrlimit(resource.RLIMIT_CORE, (0, 0))
        resource.setrlimit(resource.RLIMIT_FSIZE, (16 * 1024 * 1024,) * 2)
    p = subprocess.Popen(cmd, stdin=subprocess.PIPE, stdout=subprocess.PIPE,
                         stderr=subprocess.PIPE, preexec_fn=limits, cwd=WORK)
    timed_out = False
    try:
        out, err = p.communicate(stdin_data.encode(), timeout=timeout_s)
    except subprocess.TimeoutExpired:
        timed_out = True
        os.killpg(p.pid, signal.SIGKILL)
        out, err = p.communicate()
    trunc = lambda b: b[:max_out].decode("utf-8", "replace") + (
        "\n[output truncated]" if len(b) > max_out else "")
    code = p.returncode
    sig = -code if code is not None and code < 0 else None
    return {"stdout": trunc(out), "stderr": trunc(err),
            "exitCode": None if sig else code,
            "signal": signal.Signals(sig).name if sig else None,
            "timedOut": timed_out}


def main():
    job = json.load(sys.stdin)
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
        sources = [n for n in (list(files) + (list(tests) if test_mode else []))
                   if n.endswith((".cpp", ".cc", ".cxx"))]
        if not sources:
            result["compileOutput"] = "no .cpp files to compile"
        else:
            cmd = ["clang++", *flags, "-I/opt/zth/include", "-I" + WORK,
                   *sources, "-o", "/work/a.out"]
            t0 = time.monotonic()
            c = run_capped(cmd, "", int(lim.get("compileMs", 30000)) / 1000, 262144)
            result["compileMs"] = int((time.monotonic() - t0) * 1000)
            result["compileOutput"] = c["stderr"]
            if c["exitCode"] == 0 and not c["timedOut"]:
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
