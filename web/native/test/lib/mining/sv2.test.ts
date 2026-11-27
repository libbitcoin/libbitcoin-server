import { expect, test } from 'vitest';
import {
  frame,
  fromU256,
  MESSAGE,
  openStandardMiningChannel,
  parseFrame,
  setupConnection,
  standardHeader,
  submitSharesStandard,
  toU256,
} from '@/lib/mining/sv2';

const hex = (bytes: Uint8Array) => Array.from(bytes, (byte) => byte.toString(16).padStart(2, '0')).join('');
const bytes = (text: string) => Uint8Array.from(text.match(/../g) ?? [], (pair) => Number.parseInt(pair, 16));

test('frame sets the channel bit, type and u24 length', () => {
  expect(hex(frame(0x1a, true, new Uint8Array([1, 2, 3])))).toBe('00801a030000010203');
  expect(hex(frame(0x00, false, new Uint8Array(0)))).toBe('000000000000');
});

test('setupConnection encodes the mining protocol, versions, standard jobs flag and strings', () => {
  const encoded = setupConnection('h', 0x1234, 'v');
  expect(hex(encoded.slice(0, 6))).toBe('000000120000');
  expect(hex(encoded.slice(6))).toBe('00' + '0200' + '0200' + '01000000' + '0168' + '3412' + '0176' + '00' + '00' + '00');
});

test('openStandardMiningChannel encodes request, identity, rate and target', () => {
  const encoded = openStandardMiningChannel(7, 'w', 1, 0xffn);
  expect(hex(encoded.slice(0, 3))).toBe('000010');
  expect(hex(encoded.slice(6, 12))).toBe('07000000' + '0177');
  expect(hex(encoded.slice(12, 16))).toBe('0000803f');
  expect(hex(encoded.slice(16))).toBe('ff' + '00'.repeat(31));
});

test('submitSharesStandard encodes the six words on the channel', () => {
  const encoded = submitSharesStandard(1, 2, 3, 0xaabbccdd, 5, 6);
  expect(hex(encoded)).toBe('00801a180000' + '01000000' + '02000000' + '03000000' + 'ddccbbaa' + '05000000' + '06000000');
});

test('parseFrame reads a setup success', () => {
  expect(parseFrame(bytes('000001060000' + '0200' + '01000000'))).toEqual({ type: 'setup_success', version: 2, flags: 1 });
});

test('parseFrame reads an open success with its prefix', () => {
  const message = parseFrame(bytes('000011' + '2f0000' + '07000000' + '09000000' + 'ff' + '00'.repeat(31) + '02abcd' + '00000000'));
  expect(message).toEqual({ type: 'open_success', request: 7, channel: 9, target: 0xffn, prefix: bytes('abcd') });
});

test('parseFrame reads a future job without ntime', () => {
  const message = parseFrame(bytes('008015' + '2d0000' + '09000000' + '04000000' + '00' + '00000020' + '11'.repeat(32)));
  expect(message).toEqual({ type: 'job', channel: 9, job: 4, ntime: undefined, version: 0x20000000, merkle: bytes('11'.repeat(32)) });
});

test('parseFrame reads a new prev hash', () => {
  const message = parseFrame(bytes('008020' + '300000' + '09000000' + '04000000' + '22'.repeat(32) + '10000000' + 'ffff001d'));
  expect(message).toEqual({ type: 'prev_hash', channel: 9, job: 4, previous: bytes('22'.repeat(32)), ntime: 16, bits: 0x1d00ffff });
});

test('parseFrame reads share success and error', () => {
  expect(parseFrame(bytes('00801c' + '140000' + '09000000' + '05000000' + '03000000' + '0000000000000000'))).toEqual({ type: 'shares_success', channel: 9, last: 5, count: 3 });
  expect(parseFrame(bytes('00801d' + '0a0000' + '09000000' + '05000000' + '0278'))).toBeUndefined();
  expect(parseFrame(bytes('00801d' + '0b0000' + '09000000' + '05000000' + '027879'))).toEqual({ type: 'shares_error', channel: 9, sequence: 5, code: 'xy' });
});

test('parseFrame rejects a length that does not match the frame', () => {
  expect(parseFrame(bytes('000001070000' + '0200' + '01000000'))).toBeUndefined();
});

test('parseFrame reports an unhandled message type', () => {
  expect(parseFrame(bytes('000004000000'))).toEqual({ type: 'unknown', message: MESSAGE.setupConnection + 4 });
});

test('u256 round trips little endian', () => {
  const value = (1n << 255n) + 0x1234n;
  expect(fromU256(toU256(value))).toBe(value);
  expect(hex(toU256(0x1234n).slice(0, 2))).toBe('3412');
});

test('standardHeader lays out version, previous, merkle, time, bits and a zero nonce', () => {
  const header = standardHeader(0x20000000, bytes('aa'.repeat(32)), bytes('bb'.repeat(32)), 0x11223344, 0x1d00ffff);
  expect(header.length).toBe(80);
  expect(hex(header)).toBe('00000020' + 'aa'.repeat(32) + 'bb'.repeat(32) + '44332211' + 'ffff001d' + '00000000');
});
