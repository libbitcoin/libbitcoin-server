import { expect, test } from 'vitest';
import { formatDifficulty, formatDuration, formatHashrate } from '@/lib/mining/format';

test('formatHashrate scales units', () => {
  expect(formatHashrate(0)).toBe('0 H/s');
  expect(formatHashrate(950)).toBe('950 H/s');
  expect(formatHashrate(4_570_000)).toBe('4.57 MH/s');
  expect(formatHashrate(13.5e12)).toBe('13.5 TH/s');
});

test('formatDuration picks the largest unit', () => {
  expect(formatDuration(0.5)).toBe('under a second');
  expect(formatDuration(2.4)).toBe('2.4 seconds');
  expect(formatDuration(90)).toBe('1.5 minutes');
  expect(formatDuration(81 * 86400)).toBe('81 days');
  expect(formatDuration(Infinity)).toBe('—');
});

test('formatDifficulty', () => {
  expect(formatDifficulty(undefined)).toBe('—');
  expect(formatDifficulty(0.00064)).toBe('0.000640');
});
