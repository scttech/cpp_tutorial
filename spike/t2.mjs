import { readFile } from 'node:fs/promises';
import { fileURLToPath } from 'node:url';
const rf = globalThis.fetch;
globalThis.fetch = async (u,...r)=>String(u).startsWith('file:')?new Response(await readFile(fileURLToPath(u))):rf(u,...r);
import { compile } from 'browsercc';
const src=`#include <iostream>
#include <vector>
#include <string>
struct P{int x,y;};
int main(){ std::vector<P> v; for(int i=0;i<5;i++) v.push_back({i,i*i}); for(auto&p:v) std::cout<<p.x<<","<<p.y<<"\n"; }`;
for (let i=0;i<3;i++){const t=performance.now(); const r=await compile({source:src,fileName:'main.cpp',flags:['-std=c++20','-O0','-Wall','-Wextra','-fno-exceptions']}); console.log('compile',(performance.now()-t|0)+'ms',!!r.module, r.compileOutput.slice(0,80));}
