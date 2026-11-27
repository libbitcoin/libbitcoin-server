import { bytesToHex, hexToBytes, sha256, type Target } from './sha256';

// A stratum v1 job (mining.notify).
export interface Job {
  id: string;
  previous: string;
  prefix: string;
  suffix: string;
  branch: string[];
  version: string;
  bits: string;
  time: string;
  clean: boolean;
}

export function parseJob(params: unknown): Job | undefined {
  if (!Array.isArray(params) || params.length < 9) return undefined;
  const [id, previous, prefix, suffix, branch, version, bits, time, clean] = params as unknown[];
  const hex = (value: unknown): value is string =>
    typeof value === 'string' && /^([0-9a-f]{2})*$/i.test(value);
  if (typeof id !== 'string' || !hex(previous) || previous.length !== 64) return undefined;
  if (!hex(prefix) || !hex(suffix) || !Array.isArray(branch)) return undefined;
  if (!branch.every((hash) => hex(hash) && hash.length === 64)) return undefined;
  if (![version, bits, time].every((value) => hex(value) && value.length === 8)) return undefined;
  return {
    id,
    previous,
    prefix,
    suffix,
    branch: branch as string[],
    version: version as string,
    bits: bits as string,
    time: time as string,
    clean: clean === true,
  };
}

export function sha256d(data: Uint8Array): Uint8Array {
  return sha256(sha256(data));
}

function concat(...parts: Uint8Array[]): Uint8Array {
  const out = new Uint8Array(parts.reduce((sum, part) => sum + part.length, 0));
  let offset = 0;
  for (const part of parts) {
    out.set(part, offset);
    offset += part.length;
  }
  return out;
}

// Reverses the bytes within each 4 byte word.
function swapWords(bytes: Uint8Array): Uint8Array {
  const out = new Uint8Array(bytes.length);
  for (let i = 0; i < bytes.length; i += 4) {
    out[i] = bytes[i + 3];
    out[i + 1] = bytes[i + 2];
    out[i + 2] = bytes[i + 1];
    out[i + 3] = bytes[i];
  }
  return out;
}

// The previous block hash of a job, as displayed (reversed byte order).
export function previousHash(job: Job): string {
  return bytesToHex(swapWords(hexToBytes(job.previous)).reverse());
}

// The block height committed by the coinbase script (bip34), if any.
export function coinbaseHeight(job: Job): number | undefined {
  const bytes = hexToBytes(job.prefix);
  // version (4), input count (1), previous output (36), script size (1).
  const at = 4 + 1 + 36 + 1;
  const size = bytes[at];
  if (size === undefined || size < 1 || size > 8 || bytes.length < at + 1 + size) return undefined;
  let height = 0;
  for (let i = size; i > 0; i--) height = height * 256 + bytes[at + i];
  return height;
}

// The text pushed by the coinbase script ahead of the extranonce (the tag of
// the node), each data push after the height as printable ascii.
export function coinbaseTag(job: Job): string | undefined {
  const bytes = hexToBytes(job.prefix);
  let at = 4 + 1 + 36 + 1;
  const height = bytes[at];
  if (height === undefined || height > 8) return undefined;
  at += 1 + height;
  let text = '';
  while (at < bytes.length) {
    const size = bytes[at];
    // The extranonce push, whose data follows the prefix, ends the text.
    if (size < 1 || size > 75 || at + 1 + size > bytes.length) break;
    for (const byte of bytes.subarray(at + 1, at + 1 + size)) {
      text += byte >= 0x20 && byte <= 0x7e ? String.fromCodePoint(byte) : '';
    }
    at += 1 + size;
  }
  return text || undefined;
}

// The 80 byte header of a job and extranonce, with a zero nonce.
export function buildHeader(job: Job, extranonce1: string, extranonce2: string): Uint8Array {
  const coinbase = hexToBytes(job.prefix + extranonce1 + extranonce2 + job.suffix);
  let root = sha256d(coinbase);
  for (const hash of job.branch) root = sha256d(concat(root, hexToBytes(hash)));
  return concat(
    hexToBytes(job.version).reverse(),
    swapWords(hexToBytes(job.previous)),
    root,
    hexToBytes(job.time).reverse(),
    hexToBytes(job.bits).reverse(),
    new Uint8Array(4)
  );
}

export function withNonce(header: Uint8Array, nonce: number): Uint8Array {
  const out = header.slice();
  new DataView(out.buffer).setUint32(76, nonce, true);
  return out;
}

// The header hash as a number (little endian).
export function hashValue(header: Uint8Array): bigint {
  return BigInt(`0x${bytesToHex(sha256d(header).reverse())}`);
}

// The hash of a header, as displayed.
export function headerHash(header: Uint8Array): string {
  return bytesToHex(sha256d(header).reverse());
}

const MAX = (1n << 256n) - 1n;

// The target of difficulty one.
export const DIFFICULTY_ONE = 0xffffn << 208n;

// The share target of a (possibly fractional) difficulty.
export function difficultyTarget(difficulty: number): bigint {
  if (!(difficulty > 0) || !Number.isFinite(difficulty)) return MAX;
  // Scale the difficulty to an integer of 53 significant bits.
  const shift = 52 - Math.floor(Math.log2(difficulty));
  const scaled = BigInt(Math.floor(difficulty * 2 ** shift));
  const target =
    shift >= 0
      ? (DIFFICULTY_ONE << BigInt(shift)) / scaled
      : DIFFICULTY_ONE / (scaled << BigInt(-shift));
  return target > MAX ? MAX : target;
}

// The block target of compact bits.
export function bitsTarget(bits: string): bigint {
  const compact = Number.parseInt(bits, 16);
  const exponent = compact >>> 24;
  const mantissa = BigInt(compact & 0x007fffff);
  return exponent <= 3
    ? mantissa >> BigInt(8 * (3 - exponent))
    : mantissa << BigInt(8 * (exponent - 3));
}

// The difficulty of a target (or of a hash).
export function targetDifficulty(target: bigint): number {
  return target === 0n ? Infinity : Number(DIFFICULTY_ONE) / Number(target);
}

// A target as the eight words, most significant first, scanned by workers.
export function targetWords(target: bigint): Target {
  const words = new Uint32Array(8);
  for (let i = 0; i < 8; i++) words[i] = Number((target >> BigInt(32 * (7 - i))) & 0xffffffffn);
  return words;
}

// The hex of an extranonce2 counter of the given byte size.
export function extranonce(counter: number, size: number): string {
  return (counter >>> 0).toString(16).padStart(size * 2, '0').slice(-size * 2);
}

// Hashes per second of one browser thread, as measured.
export const THREAD_RATE = 1.2e6;

// The share difficulty that a number of threads finds about once per interval,
// suggested to the server so a browser need not wait out its vardiff decay.
export function suggestedDifficulty(threads: number, seconds = 10): number {
  return (Math.max(1, threads) * THREAD_RATE * seconds) / 2 ** 32;
}

// The params of mining.submit for a found nonce.
export function submitParams(worker: string, job: Job, extranonce2: string, nonce: number) {
  return [worker, job.id, extranonce2, job.time, (nonce >>> 0).toString(16).padStart(8, '0')];
}
