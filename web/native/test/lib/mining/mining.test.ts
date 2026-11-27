import { expect, test } from 'vitest';
import { bytesToHex } from '@/lib/mining/sha256';
import { headerWork, scan, sha256 } from '@/lib/mining/sha256';
import {
  bitsTarget,
  buildHeader,
  coinbaseHeight,
  coinbaseTag,
  difficultyTarget,
  DIFFICULTY_ONE,
  extranonce,
  headerHash,
  parseJob,
  previousHash,
  submitParams,
  suggestedDifficulty,
  targetDifficulty,
  targetWords,
  withNonce,
} from '@/lib/mining/stratum';

// The genesis coinbase, split around eight bytes taken as extranonce.
const COINBASE =
  '01000000010000000000000000000000000000000000000000000000000000000000000000ffffffff4d04ffff001d0104455468652054696d65732030332f4a616e2f32303039204368616e63656c6c6f72206f6e206272696e6b206f66207365636f6e64206261696c6f757420666f722062616e6b73ffffffff0100f2052a01000000434104678afdb0fe5548271967f1a67130b7105cd6a828e03909a67962e0ea1f61deb649f6bc3f4cef38c4f35504e51ec112de5c384df7ba0b8d578a4c702b6bf11d5fac00000000';
const SPLIT = 100;
const genesis = parseJob([
  '1',
  '0'.repeat(64),
  COINBASE.slice(0, SPLIT),
  COINBASE.slice(SPLIT + 16),
  [],
  '00000001',
  '1d00ffff',
  '495fab29',
  true,
])!;
const extranonce1 = COINBASE.slice(SPLIT, SPLIT + 8);
const extranonce2 = COINBASE.slice(SPLIT + 8, SPLIT + 16);
const NONCE = 2083236893;
const GENESIS = '000000000019d6689c085ae165831e934ff763ae46a2a6c172b3f1b60a8ce26f';

test('buildHeader assembles the genesis header from a job', () => {
  const header = buildHeader(genesis, extranonce1, extranonce2);
  expect(bytesToHex(header.slice(36, 68).reverse())).toBe(
    '4a5e1e4baab89f3a32518a88c31bc87f618f76673e2cc77ab2127b7afdeda33b'
  );
  expect(headerHash(withNonce(header, NONCE))).toBe(GENESIS);
});

test('scan finds the genesis nonce at difficulty one', () => {
  const header = buildHeader(genesis, extranonce1, extranonce2);
  const found: number[] = [];
  scan(headerWork(header), targetWords(difficultyTarget(1)), NONCE - 500, NONCE + 500, (nonce) =>
    found.push(nonce)
  );
  expect(found).toEqual([NONCE]);
});

test('scan agrees with a full hash at a low difficulty', () => {
  const header = buildHeader(genesis, extranonce1, extranonce2);
  const target = difficultyTarget(1 / 2 ** 20);
  const found: number[] = [];
  scan(headerWork(header), targetWords(target), 0, 100_000, (nonce) => found.push(nonce));
  const expected: number[] = [];
  for (let nonce = 0; nonce < 100_000; nonce++) {
    const hash = BigInt(`0x${headerHash(withNonce(header, nonce))}`);
    if (hash <= target) expected.push(nonce);
  }
  expect(expected.length).toBeGreaterThan(10);
  expect(found).toEqual(expected);
});

test('difficulty targets', () => {
  expect(difficultyTarget(1)).toBe(DIFFICULTY_ONE);
  expect(difficultyTarget(2)).toBe(DIFFICULTY_ONE / 2n);
  expect(difficultyTarget(0.5)).toBe(DIFFICULTY_ONE * 2n);
  expect(difficultyTarget(1e-80)).toBe((1n << 256n) - 1n);
  expect(bitsTarget('1d00ffff')).toBe(DIFFICULTY_ONE);
  expect(targetDifficulty(bitsTarget('1d00ffff'))).toBe(1);
  expect(Math.round(targetDifficulty(difficultyTarget(8192)))).toBe(8192);
});

// A job of the libbitcoin server on testnet5.
const job = parseJob([
  '00000000',
  '119d88b207540caca0f01b5964fe682a97ed29e40da190450000000700000000',
  '02000000010000000000000000000000000000000000000000000000000000000000000000ffffffff190277200c2f6c6962626974636f696e2f08',
  'feffffff01',
  [],
  '20000000',
  '19129f5f',
  '6927c8ff',
  false,
])!;

test('job fields as displayed', () => {
  expect(previousHash(job)).toBe('00000000000000070da1904597ed29e464fe682aa0f01b5907540cac119d88b2');
  expect(coinbaseHeight(job)).toBe(8311);
});

test('submit params encode the nonce as a big endian word', () => {
  expect(submitParams('worker', job, extranonce(1, 4), 0x0a0b0c0d)).toEqual([
    'worker',
    '00000000',
    '00000001',
    '6927c8ff',
    '0a0b0c0d',
  ]);
});

test('parseJob rejects malformed jobs', () => {
  expect(parseJob(['1'])).toBeUndefined();
  expect(parseJob(['1', 'zz', '', '', [], '00000001', '1d00ffff', '495fab29', true])).toBeUndefined();
});

test('targetDifficulty keeps small difficulties', () => {
  expect(targetDifficulty(difficultyTarget(0.00064))).toBeCloseTo(0.00064, 8);
});

test('suggestedDifficulty finds a share per interval at the thread rate', () => {
  expect(suggestedDifficulty(1, 10) * 2 ** 32).toBeCloseTo(1.2e7, 0);
  expect(suggestedDifficulty(4)).toBeCloseTo(4 * suggestedDifficulty(1), 12);
  expect(suggestedDifficulty(0)).toBe(suggestedDifficulty(1));
});

test('coinbaseTag reads the node tag ahead of the extranonce push', () => {
  expect(coinbaseTag(job)).toBe('/libbitcoin/');
  expect(coinbaseTag(genesis)).toBeUndefined();
});

test('sha256 matches the standard vectors', () => {
  expect(bytesToHex(sha256(new TextEncoder().encode('abc')))).toBe(
    'ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad'
  );
  expect(bytesToHex(sha256(new Uint8Array(0)))).toBe(
    'e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855'
  );
  expect(bytesToHex(sha256(new Uint8Array(1000).fill(97)))).toBe(
    '41edece42d63e8d9bf515a9ba6932e1c20cbc9f5a5d134645adb5db1b9737ea3'
  );
});
