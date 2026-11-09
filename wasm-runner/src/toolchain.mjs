// Compiles and links C++ with Clang/LLD running as WebAssembly (browsercc). Environment-neutral: it runs in a
// browser Worker or a Node worker thread. The caller supplies the sysroot bytes and the harness headers.
//
// Why not browsercc's own compile()? It handles one source file; lessons have several files, and test mode needs
// the learner's `main` renamed so the hidden tests can call it (what the container does with objcopy).
import { Clang, LLD, getCompilerInvocation, setUpSysroot } from "browsercc";
import { renameWasmSymbols } from "./wasmobj.mjs";

// wasm32 names `int main()` __main_void and `int main(int, char**)` __main_argc_argv.
const MAIN_SYMBOLS = { __main_void: "zth_user_main", __main_argc_argv: "zth_user_main" };
const DUMMY = "unit.cpp";
const HEADER_DIR = "/zth";

export const DEFAULT_FLAGS = ["-std=c++20", "-Wall", "-Wextra"];

/** A path inside the compiler's virtual file system must not escape it. */
const safePath = (n) => n && !n.startsWith("/") && !n.split("/").includes("..");

function rewriteCompilerArgs(args, name, obj) {
  const a = [...args];
  const set = (flag, value) => {
    const i = a.indexOf(flag);
    if (i < 0) throw new Error(`unexpected clang invocation: no ${flag}`);
    a[i + 1] = value;
  };
  set("-main-file-name", name.split("/").pop());
  set("-dumpdir", name.replace(/\.[^./]+$/, "") + "-");
  set("-o", obj);
  a[a.length - 1] = name; // the input file is the last argument
  a.splice(1, 0, "-isystem", HEADER_DIR, "-I", "/"); // like the container: -isystem /opt/zth/include (the harness is not the learner's code: no warnings from it) and -I/work
  return a;
}

export class Toolchain {
  /**
   * @param {object} o
   * @param {ArrayBuffer} o.sysroot            contents of browsercc's sysroot.tar
   * @param {Record<string,string>} o.systemHeaders  e.g. { "zth_test.hpp": "..." }, placed on the include path
   */
  constructor({ sysroot, systemHeaders }) {
    this.sysroot = sysroot;
    this.systemHeaders = systemHeaders;
    this.invocations = new Map();
    this.objectCounter = 0;
  }

  /** The clang driver's plan for these flags, computed once (asking the driver costs ~150 ms). */
  async invocation(flags) {
    const key = JSON.stringify(flags);
    let p = this.invocations.get(key);
    if (!p) {
      p = getCompilerInvocation(DUMMY, "int x;", flags);
      this.invocations.set(key, p);
    }
    return p;
  }

  /**
   * Compile one translation unit. `files` is every file the unit may include (headers and sources alike).
   * With renameMain, the object's `main` becomes `zth_user_main` (test mode, learner's files only).
   * @returns {{ok:boolean, output:string, object?:Uint8Array}}
   */
  async compileUnit({ name, files, flags = DEFAULT_FLAGS, renameMain = false }) {
    let output = "";
    try {
      const inv = await this.invocation(flags);
      const obj = `/obj/${++this.objectCounter}.o`;
      // A Clang instance can compile only once (a second callMain crashes), so every unit gets a new one.
      const clang = await Clang({
        thisProgram: "clang++",
        print: (d) => (output += d + "\n"),
        printErr: (d) => (output += d + "\n"),
      });
      setUpSysroot(clang, this.sysroot);
      clang.FS.mkdirTree(HEADER_DIR);
      clang.FS.mkdirTree("/obj");
      for (const [n, c] of Object.entries(this.systemHeaders)) clang.FS.writeFile(`${HEADER_DIR}/${n}`, c);
      for (const [n, c] of Object.entries(files)) {
        if (!safePath(n)) throw new Error(`unsafe file name: ${n}`);
        const dir = n.split("/").slice(0, -1).join("/");
        if (dir) clang.FS.mkdirTree(dir);
        clang.FS.writeFile(n, c);
      }
      const code = clang.callMain(rewriteCompilerArgs(inv.compilerArgs, name, obj));
      if (code !== 0) return { ok: false, output };
      let object = clang.FS.readFile(obj, { encoding: "binary" });
      if (renameMain) object = renameWasmSymbols(object, MAIN_SYMBOLS).bytes;
      return { ok: true, output, object };
    } catch (e) {
      // The compiler itself crashed (out of memory, native stack overflow on a pathological program...).
      return { ok: false, output: `${output}internal compiler error: ${e?.message ?? e}\n` };
    }
  }

  /**
   * Link objects into a runnable module.
   * @param {object} o
   * @param {Uint8Array[]} o.objects  compiled units, in link order
   * @param {number} [o.memoryMb]    cap on linear memory (the analogue of the container's memory limit)
   * @param {number} [o.stackKb]     size of the program's own stack (the container gives 8 MiB)
   * @returns {{ok:boolean, output:string, bytes?:Uint8Array}}
   */
  async link({ objects, flags = DEFAULT_FLAGS, memoryMb = 128, stackKb = 8192 }) {
    let output = "";
    try {
      const inv = await this.invocation(flags);
      const lld = await LLD({
        thisProgram: "wasm-ld",
        print: (d) => (output += d + "\n"),
        printErr: (d) => (output += d + "\n"),
      });
      setUpSysroot(lld, this.sysroot);
      const names = objects.map((o, i) => {
        const name = `/obj/link-${i}.o`;
        lld.FS.mkdirTree("/obj");
        lld.FS.writeFile(name, o);
        return name;
      });
      const args = inv.linkerArgs.flatMap((a) => (a === inv.compilerArtifact ? names : [a]));
      // --stack-first: running out of stack traps instead of silently overwriting the program's data.
      args.push("--stack-first", "-z", `stack-size=${stackKb * 1024}`, `--max-memory=${memoryMb * 1048576}`);
      if (lld.callMain(args) !== 0) return { ok: false, output };
      return { ok: true, output, bytes: lld.FS.readFile(inv.linerArtifact, { encoding: "binary" }) };
    } catch (e) {
      return { ok: false, output: `${output}internal linker error: ${e?.message ?? e}\n` };
    }
  }
}
