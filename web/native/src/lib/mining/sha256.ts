// SHA-256 of block headers, for the browser miner.
//
// A header is 80 bytes: its first 64 bytes are hashed once per unit of work
// (the midstate) and only the last 16 bytes, which end in the nonce, are
// hashed per nonce, followed by the hash of that 32 byte digest.

const K = new Uint32Array([
  0x428a2f98, 0x71374491, 0xb5c0fbcf, 0xe9b5dba5, 0x3956c25b, 0x59f111f1, 0x923f82a4, 0xab1c5ed5,
  0xd807aa98, 0x12835b01, 0x243185be, 0x550c7dc3, 0x72be5d74, 0x80deb1fe, 0x9bdc06a7, 0xc19bf174,
  0xe49b69c1, 0xefbe4786, 0x0fc19dc6, 0x240ca1cc, 0x2de92c6f, 0x4a7484aa, 0x5cb0a9dc, 0x76f988da,
  0x983e5152, 0xa831c66d, 0xb00327c8, 0xbf597fc7, 0xc6e00bf3, 0xd5a79147, 0x06ca6351, 0x14292967,
  0x27b70a85, 0x2e1b2138, 0x4d2c6dfc, 0x53380d13, 0x650a7354, 0x766a0abb, 0x81c2c92e, 0x92722c85,
  0xa2bfe8a1, 0xa81a664b, 0xc24b8b70, 0xc76c51a3, 0xd192e819, 0xd6990624, 0xf40e3585, 0x106aa070,
  0x19a4c116, 0x1e376c08, 0x2748774c, 0x34b0bcb5, 0x391c0cb3, 0x4ed8aa4a, 0x5b9cca4f, 0x682e6ff3,
  0x748f82ee, 0x78a5636f, 0x84c87814, 0x8cc70208, 0x90befffa, 0xa4506ceb, 0xbef9a3f7, 0xc67178f2,
]);

export const IV = new Uint32Array([
  0x6a09e667, 0xbb67ae85, 0x3c6ef372, 0xa54ff53a, 0x510e527f, 0x9b05688c, 0x1f83d9ab, 0x5be0cd19,
]);

// Compresses the 16 words at the front of w into state, writing out.
// w must hold 64 words; its tail is the message schedule.
export function compress(state: Uint32Array, w: Uint32Array, out: Uint32Array): void {
  for (let i = 16; i < 64; i++) {
    const x = w[i - 15];
    const y = w[i - 2];
    const s0 = ((x >>> 7) | (x << 25)) ^ ((x >>> 18) | (x << 14)) ^ (x >>> 3);
    const s1 = ((y >>> 17) | (y << 15)) ^ ((y >>> 19) | (y << 13)) ^ (y >>> 10);
    w[i] = (w[i - 16] + s0 + w[i - 7] + s1) | 0;
  }

  let a = state[0];
  let b = state[1];
  let c = state[2];
  let d = state[3];
  let e = state[4];
  let f = state[5];
  let g = state[6];
  let h = state[7];
  for (let i = 0; i < 64; i++) {
    const S1 = ((e >>> 6) | (e << 26)) ^ ((e >>> 11) | (e << 21)) ^ ((e >>> 25) | (e << 7));
    const t1 = (h + S1 + ((e & f) ^ (~e & g)) + K[i] + w[i]) | 0;
    const S0 = ((a >>> 2) | (a << 30)) ^ ((a >>> 13) | (a << 19)) ^ ((a >>> 22) | (a << 10));
    const t2 = (S0 + ((a & b) ^ (a & c) ^ (b & c))) | 0;
    h = g;
    g = f;
    f = e;
    e = (d + t1) | 0;
    d = c;
    c = b;
    b = a;
    a = (t1 + t2) | 0;
  }

  out[0] = state[0] + a;
  out[1] = state[1] + b;
  out[2] = state[2] + c;
  out[3] = state[3] + d;
  out[4] = state[4] + e;
  out[5] = state[5] + f;
  out[6] = state[6] + g;
  out[7] = state[7] + h;
}

function word(bytes: Uint8Array, offset: number): number {
  return (
    ((bytes[offset] << 24) |
      (bytes[offset + 1] << 16) |
      (bytes[offset + 2] << 8) |
      bytes[offset + 3]) >>>
    0
  );
}

function swap(value: number): number {
  return (
    (((value & 0xff) << 24) |
      ((value & 0xff00) << 8) |
      ((value >>> 8) & 0xff00) |
      (value >>> 24)) >>>
    0
  );
}

// The unit of work of a header: its midstate and the three words that
// precede the nonce in its second block.
export interface HeaderWork {
  midstate: Uint32Array;
  tail: Uint32Array;
}

export function headerWork(header: Uint8Array): HeaderWork {
  const w = new Uint32Array(64);
  for (let i = 0; i < 16; i++) w[i] = word(header, i * 4);
  const midstate = new Uint32Array(8);
  compress(IV, w, midstate);
  return { midstate, tail: new Uint32Array([word(header, 64), word(header, 68), word(header, 72)]) };
}

// A target as eight words, most significant first, of the hash read as a
// little endian number (as bitcoin compares proof of work).
export type Target = Uint32Array;

// Scans nonces [start, end) of the work, calling found for each nonce whose
// header hash is at or below the target.
export function scan(
  work: HeaderWork,
  target: Target,
  start: number,
  end: number,
  found: (nonce: number) => void
): void {
  const first = new Uint32Array(64);
  const second = new Uint32Array(64);
  const digest = new Uint32Array(8);
  const hash = new Uint32Array(8);
  first[0] = work.tail[0];
  first[1] = work.tail[1];
  first[2] = work.tail[2];
  first[4] = 0x80000000;
  first[15] = 640;
  const top = target[0];

  for (let nonce = start; nonce < end; nonce++) {
    first[3] = swap(nonce);
    compress(work.midstate, first, digest);
    for (let i = 0; i < 8; i++) second[i] = digest[i];
    second[8] = 0x80000000;
    for (let i = 9; i < 15; i++) second[i] = 0;
    second[15] = 256;
    compress(IV, second, hash);

    // The most significant word of the number is the last word, swapped.
    const high = swap(hash[7]);
    if (high > top) continue;
    if (high < top || below(hash, target)) found(nonce);
  }
}

// True if the hash is at or below the target.
function below(hash: Uint32Array, target: Target): boolean {
  for (let i = 0; i < 8; i++) {
    const value = swap(hash[7 - i]);
    if (value < target[i]) return true;
    if (value > target[i]) return false;
  }
  return true;
}

// SHA-256 of any message (coinbase and merkle hashing).
export function sha256(data: Uint8Array): Uint8Array {
  const blocks = Math.ceil((data.length + 9) / 64);
  const padded = new Uint8Array(blocks * 64);
  padded.set(data);
  padded[data.length] = 0x80;
  const view = new DataView(padded.buffer);
  const bits = data.length * 8;
  view.setUint32(padded.length - 8, Math.floor(bits / 2 ** 32));
  view.setUint32(padded.length - 4, bits >>> 0);
  const state = IV.slice();
  const next = new Uint32Array(8);
  const w = new Uint32Array(64);
  for (let block = 0; block < blocks; block++) {
    for (let i = 0; i < 16; i++) w[i] = view.getUint32(block * 64 + i * 4);
    compress(state, w, next);
    state.set(next);
  }
  const out = new Uint8Array(32);
  const result = new DataView(out.buffer);
  for (let i = 0; i < 8; i++) result.setUint32(i * 4, state[i]);
  return out;
}

export function bytesToHex(bytes: Uint8Array): string {
  let hex = '';
  for (const byte of bytes) hex += byte.toString(16).padStart(2, '0');
  return hex;
}

export function hexToBytes(hex: string): Uint8Array {
  if (hex.length % 2 !== 0 || !/^[0-9a-f]*$/i.test(hex)) throw new Error('Invalid hex.');
  const out = new Uint8Array(hex.length / 2);
  for (let i = 0; i < out.length; i++) out[i] = Number.parseInt(hex.slice(i * 2, i * 2 + 2), 16);
  return out;
}
