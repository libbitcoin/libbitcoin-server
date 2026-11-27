import { expect, test } from 'vitest';
import { scanModule, simdScanner } from '@/lib/mining/scan-simd';
import { headerWork, hexToBytes, scan, type Target } from '@/lib/mining/sha256';
import { difficultyTarget, targetWords } from '@/lib/mining/stratum';

const GENESIS =
  '0100000000000000000000000000000000000000000000000000000000000000000000003ba3edfd7a7b12b27ac72c3e67768f617fc81bc3888a51323a9fb8aa4b1e5e4a29ab5f49ffff001d1dac2b7c';
const NONCE = 2083236893;

function header(seed: number): Uint8Array {
  const out = new Uint8Array(80);
  let x = (seed * 2654435761) >>> 0;
  for (let i = 0; i < 80; i++) {
    x ^= x << 13;
    x >>>= 0;
    x ^= x >>> 17;
    x ^= x << 5;
    x >>>= 0;
    out[i] = x & 0xff;
  }
  return out;
}

function target(top: number): Target {
  const out = new Uint32Array(8).fill(0xffffffff);
  out[0] = top;
  return out;
}

function found(scanner: typeof scan, bytes: Uint8Array, goal: Target, start: number, end: number): number[] {
  const out: number[] = [];
  scanner(headerWork(bytes), goal, start, end, (nonce) => out.push(nonce));
  return out;
}

const simd = simdScanner();

test('scanModule is valid WebAssembly', () => {
  expect(WebAssembly.validate(scanModule())).toBe(true);
  expect(simd).toBeDefined();
});

test('simd scanner finds the genesis nonce at difficulty one', () => {
  const genesis = hexToBytes(GENESIS);
  expect(found(simd!, genesis, targetWords(difficultyTarget(1)), NONCE - 500, NONCE + 500)).toEqual([NONCE]);
});

test('simd scanner finds the nonces the javascript scanner finds', () => {
  // About one nonce in 4096 is at or below the target.
  const goal = target(0x000fffff);
  for (let seed = 1; seed <= 3; seed++) {
    const expected = found(scan, header(seed), goal, 0, 100_000);
    expect(expected.length).toBeGreaterThan(10);
    expect(found(simd!, header(seed), goal, 0, 100_000)).toEqual(expected);
  }
});

test('simd scanner keeps to ranges that are not groups of four', () => {
  const goal = target(0x0fffffff);
  for (const [start, end] of [
    [1, 2],
    [3, 9],
    [12_345, 12_350],
    [7, 70_007],
  ]) {
    expect(found(simd!, header(4), goal, start, end)).toEqual(found(scan, header(4), goal, start, end));
  }
});

test('simd scanner wraps at the end of the nonce space', () => {
  const goal = target(0x000fffff);
  const start = 2 ** 32 - 50_001;
  expect(found(simd!, header(5), goal, start, 2 ** 32)).toEqual(found(scan, header(5), goal, start, 2 ** 32));
});

test('simd scanner finds every nonce of a target every hash meets', () => {
  // Every group is a candidate, more than fit one call.
  const all = new Uint32Array(8).fill(0xffffffff);
  const nonces = found(simd!, header(6), all, 0, 40_000);
  expect(nonces.length).toBe(40_000);
  expect(nonces[39_999]).toBe(39_999);
});
