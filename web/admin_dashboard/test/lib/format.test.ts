import { expect, test } from 'vitest';
import { formatElapsed, formatIsoTimestamp } from '@/lib/format';

test('format__format_elapsed__negative__zero_seconds', () => {
  expect(formatElapsed(-5_000)).toBe('0s');
});

test('format__format_elapsed__seconds__seconds', () => {
  expect(formatElapsed(59_999)).toBe('59s');
});

test('format__format_elapsed__minutes__minutes_seconds', () => {
  expect(formatElapsed(61_000)).toBe('1m 1s');
});

test('format__format_elapsed__hours__hours_minutes', () => {
  expect(formatElapsed(3_660_000)).toBe('1h 1m');
});

test('format__format_elapsed__days__days_hours', () => {
  expect(formatElapsed(90_000_000)).toBe('1d 1h');
});

test('format__format_iso_timestamp__epoch__iso', () => {
  expect(formatIsoTimestamp(0)).toBe('1970-01-01T00:00:00.000Z');
});

test('format__format_iso_timestamp__not_finite__undefined', () => {
  expect(formatIsoTimestamp(Number.NaN)).toBeUndefined();
});
