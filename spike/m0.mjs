import { readFile } from 'node:fs/promises';
import { fileURLToPath } from 'node:url';
const realFetch = globalThis.fetch;
globalThis.fetch = async (u, ...r) => String(u).startsWith('file:')
  ? new Response(await readFile(fileURLToPath(u))) : realFetch(u, ...r);
import { compile } from 'browsercc';
import { WASI, File, OpenFile, ConsoleStdout } from '@bjorn3/browser_wasi_shim';

const tests = {
 basic: `#include <iostream>
#include <vector>
#include <string>
#include <algorithm>
#include <memory>
#include <map>
#include <optional>
#include <variant>
#include <ranges>
#include <format>
int main(){
  std::vector<int> v{5,3,1,4};
  std::ranges::sort(v);
  auto p = std::make_unique<std::string>("hi");
  std::map<std::string,int> m{{"a",1}};
  std::optional<int> o = 3;
  for(int x: v | std::views::filter([](int i){return i>2;})) std::cout<<x<<' ';
  std::cout<<*p<<m["a"]<<*o<<std::format(" {:>5}", 42)<<std::endl;
}`,
 exceptions: `#include <iostream>
#include <stdexcept>
int main(){ try { throw std::runtime_error("boom"); } catch(const std::exception& e){ std::cout<<e.what()<<std::endl; } }`,
 error: `int main(){ int x = "s"; undefined_fn(); }`,
};
for (const [name, source] of Object.entries(tests)) {
  for (const flags of [['-std=c++20','-O0','-Wall','-Wextra','-fno-exceptions']]) {
    const t0 = performance.now();
    let r;
    try { r = await compile({ source, fileName: 'main.cpp', flags }); } catch (e) { console.log(`[${name}] compile threw:`, String(e).slice(0,300)); continue; }
    const t1 = performance.now();
    console.log(`[${name}] compile ${(t1-t0|0)}ms ok=${!!r.module}`);
    if (!r.module) { console.log(r.compileOutput.slice(0,600)); continue; }
    let out='';
    const fds=[new OpenFile(new File(new Uint8Array())),new ConsoleStdout(d=>out+=new TextDecoder().decode(d)),new ConsoleStdout(d=>out+=new TextDecoder().decode(d))];
    const wasi=new WASI([ 'a.out' ],[],fds);
    try { const inst=await WebAssembly.instantiate(r.module,{wasi_snapshot_preview1:wasi.wasiImport}); const code=wasi.start(inst); console.log(`  run exit=${code} out=${JSON.stringify(out)} ${(performance.now()-t1|0)}ms`);} catch(e){ console.log('  run threw', String(e).slice(0,200), JSON.stringify(out)); }
  }
}
