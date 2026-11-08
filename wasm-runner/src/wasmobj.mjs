// Rename function symbols inside a relocatable WebAssembly object file (what `objcopy --redefine-sym` does for ELF).
// Only the "linking" custom section's symbol table is touched; relocations refer to symbols by index, so they stay valid.
const dec = new TextDecoder(), enc = new TextEncoder();

function readLEB(b, st) {
  let r = 0, shift = 0, x;
  do { x = b[st.p++]; r += (x & 0x7f) * 2 ** shift; shift += 7; } while (x & 0x80);
  return r;
}
function lebBytes(n) {
  const out = [];
  do { let x = n % 128; n = Math.floor(n / 128); if (n) x |= 0x80; out.push(x); } while (n);
  return out;
}
const concat = (chunks) => {
  const out = new Uint8Array(chunks.reduce((n, c) => n + c.length, 0));
  let o = 0;
  for (const c of chunks) { out.set(c, o); o += c.length; }
  return out;
};

const SYMTAB = 8, UNDEFINED = 0x10, EXPLICIT_NAME = 0x40;
const K_FUNCTION = 0, K_DATA = 1, K_GLOBAL = 2, K_SECTION = 3, K_TAG = 4, K_TABLE = 5;

function renameInSymtab(payload, map) {
  const st = { p: 0 };
  const count = readLEB(payload, st);
  const chunks = [new Uint8Array(lebBytes(count))];
  let copied = st.p, renamed = 0;
  const flush = (to) => { if (to > copied) chunks.push(payload.subarray(copied, to)); copied = to; };
  for (let i = 0; i < count; i++) {
    const kind = payload[st.p++];
    const flags = readLEB(payload, st);
    if (kind === K_FUNCTION || kind === K_GLOBAL || kind === K_TAG || kind === K_TABLE) {
      readLEB(payload, st); // index
      if (!(flags & UNDEFINED) || flags & EXPLICIT_NAME) {
        const lenStart = st.p;
        const len = readLEB(payload, st);
        const name = dec.decode(payload.subarray(st.p, st.p + len));
        const nameEnd = st.p + len;
        if (kind === K_FUNCTION && !(flags & UNDEFINED) && name in map) {
          flush(lenStart);
          const nb = enc.encode(map[name]);
          chunks.push(new Uint8Array([...lebBytes(nb.length), ...nb]));
          copied = nameEnd;
          renamed++;
        }
        st.p = nameEnd;
      }
    } else if (kind === K_DATA) {
      const len = readLEB(payload, st);
      st.p += len;
      if (!(flags & UNDEFINED)) { readLEB(payload, st); readLEB(payload, st); readLEB(payload, st); }
    } else if (kind === K_SECTION) {
      readLEB(payload, st);
    } else {
      throw new Error(`unknown wasm symbol kind ${kind}`);
    }
  }
  flush(payload.length);
  return { payload: concat(chunks), renamed };
}

/** Returns { bytes, renamed }: the object with each defined function named in `map` renamed. */
export function renameWasmSymbols(obj, map) {
  const b = obj instanceof Uint8Array ? obj : new Uint8Array(obj);
  if (b[0] !== 0 || b[1] !== 0x61 || b[2] !== 0x73 || b[3] !== 0x6d) throw new Error('not a wasm object');
  const out = [b.subarray(0, 8)];
  const st = { p: 8 };
  let renamed = 0;
  while (st.p < b.length) {
    const secStart = st.p;
    const id = b[st.p++];
    const size = readLEB(b, st);
    const bodyStart = st.p, bodyEnd = bodyStart + size;
    let handled = false;
    if (id === 0) {
      const nlen = readLEB(b, st);
      const name = dec.decode(b.subarray(st.p, st.p + nlen));
      if (name === 'linking') {
        const headEnd = st.p + nlen;
        const s2 = { p: headEnd };
        readLEB(b, s2); // version
        const head = b.subarray(bodyStart, s2.p);
        const parts = [head];
        while (s2.p < bodyEnd) {
          const type = b[s2.p++];
          const sz = readLEB(b, s2);
          const sub = b.subarray(s2.p, s2.p + sz);
          s2.p += sz;
          let body = sub;
          if (type === SYMTAB) {
            const r = renameInSymtab(sub, map);
            body = r.payload;
            renamed += r.renamed;
          }
          parts.push(new Uint8Array([type, ...lebBytes(body.length)]), body);
        }
        const newBody = concat(parts);
        out.push(new Uint8Array([0, ...lebBytes(newBody.length)]), newBody);
        handled = true;
      }
    }
    if (!handled) out.push(b.subarray(secStart, bodyEnd));
    st.p = bodyEnd;
  }
  return { bytes: concat(out), renamed };
}
