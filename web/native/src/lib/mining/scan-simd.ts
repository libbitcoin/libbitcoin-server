import { scan, type HeaderWork, type Target } from './sha256';

// A WebAssembly header scanner using 128-bit SIMD: each of the four lanes
// hashes its own nonce, so one pass checks four consecutive nonces. The
// module is generated here, as bytes, from the same round function as the
// JavaScript scanner, with what is fixed folded away:
//  - values fixed by SHA-256 (padding, initial state, round constants) are
//    computed while generating,
//  - values fixed by the unit of work (midstate, tail) once per scan,
//  - only what depends on the nonce in the loop,
//  - the second hash stops after round 60, which fixes the word compared.
// The module only finds groups of four nonces with a candidate in the most
// significant word; each candidate is confirmed by the JavaScript scanner.
//
// Memory: midstate words at 0, tail words at 32, starts of candidate groups
// from 64. Export scan(start, groups, top) returns the number of candidates.

const K = [
  0x428a2f98, 0x71374491, 0xb5c0fbcf, 0xe9b5dba5, 0x3956c25b, 0x59f111f1, 0x923f82a4, 0xab1c5ed5,
  0xd807aa98, 0x12835b01, 0x243185be, 0x550c7dc3, 0x72be5d74, 0x80deb1fe, 0x9bdc06a7, 0xc19bf174,
  0xe49b69c1, 0xefbe4786, 0x0fc19dc6, 0x240ca1cc, 0x2de92c6f, 0x4a7484aa, 0x5cb0a9dc, 0x76f988da,
  0x983e5152, 0xa831c66d, 0xb00327c8, 0xbf597fc7, 0xc6e00bf3, 0xd5a79147, 0x06ca6351, 0x14292967,
  0x27b70a85, 0x2e1b2138, 0x4d2c6dfc, 0x53380d13, 0x650a7354, 0x766a0abb, 0x81c2c92e, 0x92722c85,
  0xa2bfe8a1, 0xa81a664b, 0xc24b8b70, 0xc76c51a3, 0xd192e819, 0xd6990624, 0xf40e3585, 0x106aa070,
  0x19a4c116, 0x1e376c08, 0x2748774c, 0x34b0bcb5, 0x391c0cb3, 0x4ed8aa4a, 0x5b9cca4f, 0x682e6ff3,
  0x748f82ee, 0x78a5636f, 0x84c87814, 0x8cc70208, 0x90befffa, 0xa4506ceb, 0xbef9a3f7, 0xc67178f2,
];
const IV = [0x6a09e667, 0xbb67ae85, 0x3c6ef372, 0xa54ff53a, 0x510e527f, 0x9b05688c, 0x1f83d9ab, 0x5be0cd19];

// Instructions used (opcodes, those after 0xfd are SIMD).
const LOCAL_GET = 0x20;
const LOCAL_SET = 0x21;
const I32_CONST = 0x41;
const I32_EQZ = 0x45;
const I32_ADD = 0x6a;
const I32_SUB = 0x6b;
const I32_SHL = 0x74;
const I32_STORE = 0x36;
const BLOCK = 0x02;
const LOOP = 0x03;
const IF = 0x04;
const END = 0x0b;
const BR = 0x0c;
const BR_IF = 0x0d;
const EMPTY = 0x40;
const I32 = 0x7f;
const V128 = 0x7b;
const LOAD32_SPLAT = 0x09;
const V128_CONST = 0x0c;
const SHUFFLE = 0x0d;
const SPLAT = 0x11;
const EXTRACT_LANE = 0x1b;
const LE_U = 0x3e;
const OR = 0x50;
const XOR = 0x51;
const BITSELECT = 0x52;
const ANY_TRUE = 0x53;
const SHL = 0xab;
const SHR_U = 0xad;
const ADD = 0xae;

type Bytes = number[];

function uleb(value: number): Bytes {
  const out: Bytes = [];
  do {
    let byte = value & 0x7f;
    value >>>= 7;
    if (value) byte |= 0x80;
    out.push(byte);
  } while (value);
  return out;
}

function sleb(value: number): Bytes {
  const out: Bytes = [];
  for (;;) {
    const byte = value & 0x7f;
    value >>= 7;
    if ((value === 0 && !(byte & 0x40)) || (value === -1 && byte & 0x40)) {
      out.push(byte);
      return out;
    }
    out.push(byte | 0x80);
  }
}

const simd = (op: number): Bytes => [0xfd, ...uleb(op)];
const i32 = (value: number): Bytes => [I32_CONST, ...sleb(value | 0)];
const get = (index: number): Bytes => [LOCAL_GET, ...uleb(index)];
const set = (index: number): Bytes => [LOCAL_SET, ...uleb(index)];
const splat = (value: number): Bytes => {
  const lane = [value & 0xff, (value >>> 8) & 0xff, (value >>> 16) & 0xff, (value >>> 24) & 0xff];
  return [...simd(V128_CONST), ...lane, ...lane, ...lane, ...lane];
};
const lanes = (values: number[]): Bytes => [
  ...simd(V128_CONST),
  ...values.flatMap((v) => [v & 0xff, (v >>> 8) & 0xff, (v >>> 16) & 0xff, (v >>> 24) & 0xff]),
];
// Swaps the bytes of each lane.
const swap = (x: Bytes): Bytes => [...x, ...x, ...simd(SHUFFLE), 3, 2, 1, 0, 7, 6, 5, 4, 11, 10, 9, 8, 15, 14, 13, 12];

// A value is fixed (c, a number), fixed per scan (j) or per nonce (n), the
// latter two held in locals.
type Value = { k: 'c'; v: number } | { k: 'j' | 'n'; local: number };

const PARAMS = 3; // start, groups, top
const FOUND = 3;
const MIDSTATE = 4; // 8 locals
const TAIL = 12; // 3 locals
const NONCE = 15;
const LANES = 16;
const TOP = 17;
const HIGH = 18;
const LOCALS = 19;

// The bytes of the scanner module.
export function scanModule(): Uint8Array<ArrayBuffer> {
  const job: Bytes = [];
  const loop: Bytes = [];
  let locals = LOCALS;

  const C = (v: number): Value => ({ k: 'c', v: v >>> 0 });
  const rank = { c: 0, j: 1, n: 2 };
  const kindOf = (xs: Value[]) => xs.reduce<Value['k']>((k, x) => (rank[x.k] > rank[k] ? x.k : k), 'c');
  const code = (x: Value): Bytes => (x.k === 'c' ? splat(x.v) : get(x.local));
  const bind = (k: 'j' | 'n', expr: Bytes): Value => {
    const local = locals++;
    (k === 'j' ? job : loop).push(...expr, ...set(local));
    return { k, local };
  };
  const op = (fixed: (...v: number[]) => number, expr: (...x: Bytes[]) => Bytes, ...xs: Value[]): Value => {
    const k = kindOf(xs);
    if (k === 'c') return C(fixed(...xs.map((x) => (x as { v: number }).v)));
    return bind(k, expr(...xs.map(code)));
  };

  const rotr = (x: number, n: number) => ((x >>> n) | (x << (32 - n))) >>> 0;
  const vrotr = (x: Bytes, n: number): Bytes => [
    ...x, ...i32(n), ...simd(SHR_U), ...x, ...i32(32 - n), ...simd(SHL), ...simd(OR),
  ];
  const vshr = (x: Bytes, n: number): Bytes => [...x, ...i32(n), ...simd(SHR_U)];
  const vxor = (...xs: Bytes[]): Bytes => xs.reduce((a, x) => [...a, ...x, ...simd(XOR)]);

  const s0 = (x: Value) =>
    op((x) => rotr(x, 7) ^ rotr(x, 18) ^ (x >>> 3), (x) => vxor(vrotr(x, 7), vrotr(x, 18), vshr(x, 3)), x);
  const s1 = (x: Value) =>
    op((x) => rotr(x, 17) ^ rotr(x, 19) ^ (x >>> 10), (x) => vxor(vrotr(x, 17), vrotr(x, 19), vshr(x, 10)), x);
  const S0 = (x: Value) =>
    op((x) => rotr(x, 2) ^ rotr(x, 13) ^ rotr(x, 22), (x) => vxor(vrotr(x, 2), vrotr(x, 13), vrotr(x, 22)), x);
  const S1 = (x: Value) =>
    op((x) => rotr(x, 6) ^ rotr(x, 11) ^ rotr(x, 25), (x) => vxor(vrotr(x, 6), vrotr(x, 11), vrotr(x, 25)), x);
  // ch takes f where e is set, else g; maj takes b where b and c agree, else a.
  const ch = (e: Value, f: Value, g: Value) =>
    op((e, f, g) => (e & f) ^ (~e & g), (e, f, g) => [...f, ...g, ...e, ...simd(BITSELECT)], e, f, g);
  const maj = (a: Value, b: Value, c: Value) =>
    op(
      (a, b, c) => (a & b) ^ (a & c) ^ (b & c),
      (a, b, c) => [...a, ...b, ...b, ...c, ...simd(XOR), ...simd(BITSELECT)],
      a,
      b,
      c
    );

  // Sums fold fixed terms and compute the part fixed per scan once.
  const add = (...xs: Value[]): Value => {
    let fixed = 0;
    const perJob: Value[] = [];
    const perNonce: Value[] = [];
    for (const x of xs) {
      if (x.k === 'c') fixed = (fixed + x.v) >>> 0;
      else (x.k === 'j' ? perJob : perNonce).push(x);
    }
    const sum = (terms: Bytes[]) => terms.reduce((a, t) => [...a, ...t, ...simd(ADD)]);
    let jobPart: Value | null = null;
    if (perJob.length) {
      const terms = perJob.map(code);
      if (fixed) terms.push(splat(fixed));
      jobPart = terms.length === 1 ? perJob[0] : bind('j', sum(terms));
    }
    if (!perNonce.length) return jobPart ?? C(fixed);
    const terms = perNonce.map(code);
    if (jobPart) terms.push(code(jobPart));
    else if (fixed) terms.push(splat(fixed));
    return terms.length === 1 ? perNonce[0] : bind('n', sum(terms));
  };

  // Compresses words w into state s up to the last round, a computed while
  // needed; returns the working state.
  const compress = (s: Value[], w: Value[], last: number, needA: (round: number) => boolean) => {
    for (let i = 16; i <= last; i++) w[i] = add(s1(w[i - 2]), w[i - 7], s0(w[i - 15]), w[i - 16]);
    let [a, b, c, d, e, f, g, h] = s;
    for (let i = 0; i <= last; i++) {
      const t1 = add(h, S1(e), ch(e, f, g), C(K[i]), w[i]);
      const next = needA(i) ? add(t1, S0(a), maj(a, b, c)) : C(0);
      h = g;
      g = f;
      f = e;
      e = add(d, t1);
      d = c;
      c = b;
      b = a;
      a = next;
    }
    return [a, b, c, d, e, f, g, h];
  };

  const J = (local: number): Value => ({ k: 'j', local });
  const midstate = [0, 1, 2, 3, 4, 5, 6, 7].map((i) => J(MIDSTATE + i));
  const first: Value[] = [
    J(TAIL), J(TAIL + 1), J(TAIL + 2), { k: 'n', local: NONCE },
    C(0x80000000), ...Array<Value>(10).fill(C(0)), C(640),
  ];
  const state = compress(midstate, first, 63, () => true);
  const digest = state.map((x, i) => add(midstate[i], x));
  const second: Value[] = [...digest, C(0x80000000), ...Array<Value>(6).fill(C(0)), C(256)];
  // Word 7 of the hash is its initial word plus e after round 60, which reads
  // a of round 57 at the latest.
  const final = compress(IV.map(C), second, 60, (round) => round <= 57);
  const word = add(C(IV[7]), final[4]);

  const body: Bytes = [
    ...[0, 1, 2, 3, 4, 5, 6, 7].flatMap((i) => [...i32(i * 4), ...simd(LOAD32_SPLAT), 2, 0, ...set(MIDSTATE + i)]),
    ...[0, 1, 2].flatMap((i) => [...i32(32 + i * 4), ...simd(LOAD32_SPLAT), 2, 0, ...set(TAIL + i)]),
    ...get(2), ...simd(SPLAT), ...set(TOP),
    ...get(0), ...simd(SPLAT), ...lanes([0, 1, 2, 3]), ...simd(ADD), ...set(LANES),
    ...job,
    BLOCK, EMPTY,
    LOOP, EMPTY,
    ...get(1), I32_EQZ, BR_IF, 1,
    ...swap(get(LANES)), ...set(NONCE),
    ...loop,
    ...swap(code(word)), ...set(HIGH),
    ...get(HIGH), ...get(TOP), ...simd(LE_U), ...simd(ANY_TRUE),
    IF, EMPTY,
    ...i32(64), ...get(FOUND), ...i32(2), I32_SHL, I32_ADD,
    ...get(LANES), ...simd(EXTRACT_LANE), 0,
    I32_STORE, 2, 0,
    ...get(FOUND), ...i32(1), I32_ADD, ...set(FOUND),
    END,
    ...get(LANES), ...lanes([4, 4, 4, 4]), ...simd(ADD), ...set(LANES),
    ...get(1), ...i32(1), I32_SUB, ...set(1),
    BR, 0,
    END,
    END,
    ...get(FOUND),
    END,
  ];
  const declarations = [...uleb(2), 1, I32, ...uleb(locals - PARAMS - 1), V128];
  const func = [...declarations, ...body];

  const section = (id: number, content: Bytes): Bytes => [id, ...uleb(content.length), ...content];
  const name = (text: string): Bytes => [...uleb(text.length), ...Array.from(text, (c) => c.charCodeAt(0))];
  return new Uint8Array([
    0x00, 0x61, 0x73, 0x6d, 0x01, 0x00, 0x00, 0x00,
    ...section(1, [1, 0x60, 3, I32, I32, I32, 1, I32]),
    ...section(3, [1, 0]),
    ...section(5, [1, 0x00, 1]),
    ...section(7, [2, ...name('memory'), 2, 0, ...name('scan'), 0, 0]),
    ...section(10, [1, ...uleb(func.length), ...func]),
  ]);
}

export type Scanner = typeof scan;

// Groups of four per call, so that candidates (at most one per group) fit
// in the memory after 64 even when every group has one.
const CHUNK = 1 << 14;

// The SIMD scanner, or undefined where WebAssembly SIMD is not supported.
export function simdScanner(): Scanner | undefined {
  try {
    const bytes = scanModule();
    if (typeof WebAssembly === 'undefined' || !WebAssembly.validate(bytes)) return undefined;
    const instance = new WebAssembly.Instance(new WebAssembly.Module(bytes));
    const exports = instance.exports as { memory: WebAssembly.Memory; scan: (start: number, groups: number, top: number) => number };
    const memory = new Int32Array(exports.memory.buffer);
    return (work: HeaderWork, target: Target, start: number, end: number, found: (nonce: number) => void) => {
      memory.set(work.midstate, 0);
      memory.set(work.tail, 8);
      for (let from = start; from < end; from += CHUNK) {
        const count = Math.min(end - from, CHUNK);
        const groups = exports.scan(from | 0, Math.ceil(count / 4), target[0] | 0);
        for (let candidate = 0; candidate < groups; candidate++) {
          for (let lane = 0; lane < 4; lane++) {
            const nonce = (memory[16 + candidate] + lane) >>> 0;
            if (((nonce - from) >>> 0) < count) scan(work, target, nonce, nonce + 1, found);
          }
        }
      }
    };
  } catch {
    return undefined;
  }
}
